"""Load, dump and switch the FeRAM training slots of the PneutouchAi board (PST1).

The chip keeps up to 4 slots of raw training records (label + 14 pool values)
in its FeRAM. At boot it ranks the features itself, normalizes and trains
Solist-AI from the active slot; see docs/model-store.md.

    .venv/bin/python tools/model_store.py --port COM7 info
    .venv/bin/python tools/model_store.py --port COM7 load --slot 0 --input training.json [--k 0] [--name t] [--use]
    .venv/bin/python tools/model_store.py --port COM7 dump --slot 0 --out slot0.json
    .venv/bin/python tools/model_store.py --port COM7 use --slot 1
    .venv/bin/python tools/model_store.py --port COM7 erase --slot 1
    .venv/bin/python tools/model_store.py --port COM7 train
    .venv/bin/python tools/model_store.py --port COM7 tune
    .venv/bin/python tools/model_store.py --port COM7 selftest
"""
import argparse
import contextlib
import json
import math
import os
import sys
from datetime import date
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import feature_select as fs  # noqa: E402

FEATURE_ORDER = fs.FEATURE_NAMES[:12]
ZONE_NAMES = {1: 'TAIL', 2: 'BACK', 3: 'LEGS', 4: 'HEAD'}
SLOTS, MAX_RECORDS, MIN_RECORDS, MAX_NAME = 4, 1000, 4, 8
STATES = {0: 'empty', 1: 'open', 2: 'committed'}
TRAIN_TIMEOUT_S = 120  # 1000 records x 4 epochs at ~1.5 ms per step, plus FeRAM reads
TUNE_TIMEOUT_S = 900  # ~2 min at 176 records (99 subsets x 2 screening passes + 6 x 4 folds)
TUNE_FOLDS, ADVICE_RECALL_PCT, RECALL_ABSENT = 4, 90, 255
TUNE_MIN_SIZE, SCREEN_PER_CLASS, SCREEN_PASSES, TUNE_MARGIN_PCT = 3, 10, 2, 2  # model_tune.h
MODEL_ERRORS = {1: 'STORE', 2: 'NO_SLOT', 3: 'RECORD', 4: 'SELECT', 5: 'AI'}
ZONE_JA = {'TAIL': 'しっぽ', 'BACK': '背中', 'LEGS': '足', 'HEAD': '頭'}


def encode_pool(pool):
    """14 float32 as 112 hex digits, high nibble first (PAI1 LIVEPREDICT style)."""
    a = np.asarray(pool, dtype=np.float32)
    if a.shape != (fs.POOL_COUNT,) or not (np.isfinite(a).all() and (a > 0).all()):
        raise ValueError('pool must be 14 finite values > 0')
    return ''.join(f'{int(b):08x}' for b in a.view(np.uint32))


def decode_pool(text):
    if len(text) != fs.POOL_COUNT * 8:
        raise ValueError(f'pool hex must be {fs.POOL_COUNT * 8} digits')
    bits = np.array([int(text[i:i + 8], 16) for i in range(0, len(text), 8)], dtype=np.uint32)
    return bits.view(np.float32)


def check_name(name):
    if len(name) > MAX_NAME or not all(' ' <= c <= '~' and c not in ',*' for c in name):
        raise ValueError(f'name must be up to {MAX_NAME} printable characters without , or *')
    return name


def check_slot(slot):
    if not 0 <= slot < SLOTS:
        raise ValueError(f'slot must be 0..{SLOTS - 1}')
    return slot


def _indices(text):
    return [int(v) for v in text.split(':')] if text else []


def mask_features(mask):
    """Pool indices of a feature mask (bit j = pool feature j), ascending."""
    return [j for j in range(fs.POOL_COUNT) if mask >> j & 1]


def _finalists(text):
    """'mask=screen=hits:...' -> [dict(mask, screen, correct)]."""
    entries = [item.split('=') for item in text.split(':')] if text else []
    return [dict(mask=int(m, 16), screen=int(sc), correct=int(c)) for m, sc, c in entries]


class Store:
    """PST1 commands over a validate_solist_ai.Device (or any object with the same command())."""

    def __init__(self, device):
        self.device = device

    def _cmd(self, operation, *fields, timeout=8):
        self.device.token += 1
        return self.device.command(operation, self.device.token, *fields, timeout=timeout, family='PST1')[3:]

    def mode(self, on):
        self.device.command('MODE', 1 if on else 0)

    @contextlib.contextmanager
    def paused(self):
        """MODE 1 (acquisition paused) for the body; MODE 0 always restores live mode."""
        self.mode(True)
        try:
            yield self
        finally:
            self.mode(False)

    def info(self):
        r = self._cmd('INFO')
        return dict(active=int(r[0]), counts=[int(v) for v in r[1:5]], firmware=r[5] if len(r) > 5 else '')

    def slot(self, slot):
        r = self._cmd('SLOT', check_slot(slot))
        # tuned_k and recall were added with TUNE, mask with the subset search; older firmware stops earlier.
        return dict(slot=int(r[0]), state=int(r[1]), count=int(r[2]), pool=int(r[3]), k=int(r[4]), name=r[5],
                    tuned_k=int(r[6]) if len(r) > 6 else 0,
                    recall=_indices(r[7]) if len(r) > 7 else [0, 0, 0, 0],
                    mask=int(r[8], 16) if len(r) > 8 else 0)

    def open(self, slot, k, name):
        self._cmd('OPEN', check_slot(slot), k, check_name(name))

    def put(self, slot, label, pool):
        return int(self._cmd('PUT', slot, label, encode_pool(pool))[1])

    def commit(self, slot):
        return int(self._cmd('COMMIT', slot)[1])

    def get(self, slot, index):
        r = self._cmd('GET', slot, index)
        return int(r[2]), decode_pool(r[3])

    def erase(self, slot):
        self._cmd('ERASE', check_slot(slot))

    def use(self, slot):
        self._cmd('USE', check_slot(slot))

    def train(self):
        r = self._cmd('TRAIN', timeout=TRAIN_TIMEOUT_S)
        return dict(ready=r[0] == '1', error=int(r[1]), count=int(r[2]), k=int(r[3]),
                    selected=_indices(r[4]), training_us=int(r[5]))

    def tune(self):
        """Subset search on the chip (screening + 4-fold finals); writes mask/recall into the active slot header."""
        r = self._cmd('TUNE', timeout=TUNE_TIMEOUT_S)
        confusion = _indices(r[13])
        return dict(ok=r[0] == '1', error=int(r[1]), count=int(r[2]), mask=int(r[3], 16), best_k=int(r[4]),
                    correct=int(r[5]), champion_mask=int(r[6], 16), champion_correct=int(r[7]),
                    finalists=_finalists(r[8]), candidates=int(r[9]), screened=int(r[10]),
                    recall=_indices(r[11]), counts=_indices(r[12]),
                    confusion=[confusion[i:i + 4] for i in range(0, 16, 4)],
                    advice=int(r[14]), elapsed_us=int(r[15]))

    def selftest(self):
        return int(self._cmd('SELFTEST')[0])


def records(training):
    """(labels, float32 pool rows) from a prepare_retrain / dump JSON; validated like the chip."""
    events = training['events']
    labels = [int(e['label']) for e in events]
    pool = fs.pool_rows(events)
    if not MIN_RECORDS <= len(events) <= MAX_RECORDS:
        raise ValueError(f'{len(events)} events; a slot holds {MIN_RECORDS}..{MAX_RECORDS}')
    if any(label not in ZONE_NAMES for label in labels) or not (np.isfinite(pool).all() and (pool > 0).all()):
        raise ValueError('labels must be 1..4 and every pool value finite and > 0')
    return labels, pool


def load(store, slot, training, k=0, name='', use=False):
    """Write every event, verify the first and last record bit-exactly, then TRAIN."""
    labels, pool = records(training)
    check_slot(slot); check_name(name)
    if not 0 <= k <= fs.SELECT_MAX:
        raise ValueError('k must be 0..12')
    with store.paused():
        store.open(slot, k, name)
        for label, row in zip(labels, pool):
            store.put(slot, label, row)
        if store.commit(slot) != len(labels):
            raise RuntimeError('record count mismatch after COMMIT')
        for index in sorted({0, len(labels) - 1}):
            label, row = store.get(slot, index)
            if label != labels[index] or not np.array_equal(row.view(np.uint32), pool[index].view(np.uint32)):
                raise RuntimeError(f'read-back mismatch at record {index}')
        if use:
            store.use(slot)
        return store.train()


def dump(store, slot):
    """Same JSON shape as prepare_retrain (feature_order, events) plus the FeRAM source."""
    events = []
    with store.paused():
        info = store.slot(slot)
        for index in range(info['count'] if info['state'] else 0):
            label, row = store.get(slot, index)
            events.append(dict(label=label, zone=ZONE_NAMES[label], features12=[float(v) for v in row[:12]],
                               peak=float(row[12]), trough=float(row[13])))
    return dict(feature_order=FEATURE_ORDER, events=events,
                source=dict(kind='fram', slot=slot, name=info['name'], k=info['k'], state=STATES[info['state']],
                            tuned_k=info['tuned_k'], recall=info['recall'], mask=info['mask']))


def describe_training(result):
    names = ', '.join(fs.FEATURE_NAMES[j] for j in result['selected']) or 'none'
    status = 'READY' if result['ready'] else f'ERROR {result["error"]}'
    return (f'{status}: {result["count"]} records, {result["k"]} features selected ({names}), '
            f'training {result["training_us"] / 1e6:.2f} s')


def _zone(label):
    name = ZONE_NAMES[label]
    return f'{ZONE_JA[name]}（{name}）'


def _names(mask):
    return ', '.join(fs.FEATURE_NAMES[j] for j in mask_features(mask)) or 'none'


def shortlist_size(candidates):
    """Shortlist size whose subsets of TUNE_MIN_SIZE.. members number `candidates` (0 if none)."""
    for size in range(TUNE_MIN_SIZE, fs.SELECT_MAX + 1):
        if sum(math.comb(size, k) for k in range(TUNE_MIN_SIZE, size + 1)) == candidates:
            return size
    return 0


def tune_margin(count):
    """Records a challenger must win by more than (model_tune.c margin())."""
    return max(1, (count * TUNE_MARGIN_PCT + 50) // 100)


def _tune_summary(result):
    count, champion = result['count'], result['champion_mask']
    finals = 1 + sum(f['mask'] != champion for f in result['finalists'])
    judged = result['screened'] * SCREEN_PASSES
    lines = [f'tune: {count} records, shortlist {shortlist_size(result["candidates"])} features, '
             f'{result["candidates"]} subsets screened ({SCREEN_PER_CLASS}/class × {SCREEN_PASSES} passes), '
             f'{finals} finalists ({result["elapsed_us"] / 1e6:.1f} s)',
             f'  champion (top-{len(mask_features(champion))}): {_names(champion)}  4-fold '
             f'{result["champion_correct"]}/{count} = {100 * result["champion_correct"] / count:.1f}%']
    for i, f in enumerate(result['finalists']):
        lines.append(f'  {"finalists:" if i == 0 else "":<10}  {len(mask_features(f["mask"]))} features '
                     f'{_names(f["mask"])}   screen {f["screen"]}/{judged}  4-fold {f["correct"]}/{count}')
    if result['mask'] == champion:
        lines.append(f'  chosen: champion (no challenger beat it by more than {tune_margin(count)} records)')
    else:
        lines.append(f'  chosen: {result["best_k"]} features {_names(result["mask"])} '
                     f'(beat champion by {result["correct"] - result["champion_correct"]})')
    return lines


def describe_tune(result):
    """Lines for the CLI; RuntimeError (error code name) when the chip could not tune."""
    if not result['ok']:
        raise RuntimeError(f'TUNE failed: {MODEL_ERRORS.get(result["error"], result["error"])}')
    lines = _tune_summary(result) + [
        '  ' + '  '.join(f'{ZONE_NAMES[c + 1]} {"-" if r == RECALL_ABSENT else f"{r}%"} ({n})'
                         for c, (r, n) in enumerate(zip(result['recall'], result['counts'])))]
    lines += confusion_table(result['confusion'])
    advice = result['advice']
    if not advice:
        return lines + [f'  提案: なし（すべての部位が {ADVICE_RECALL_PCT}% 以上）']
    return lines + _advice_lines(result['confusion'], advice)


def confusion_table(confusion):
    """Chosen set's 4x4 matrix: rows = true class, columns = predicted class."""
    names = [ZONE_NAMES[c + 1] for c in range(4)]
    return (['  混同行列（行 = 正解、列 = 判定）', '        ' + ' '.join(names)]
            + [f'  {names[t]:<4}  ' + ' '.join(f'{n:>4}' for n in row) for t, row in enumerate(confusion)])


def _advice_lines(confusion, advice):
    """Both directions of the advised class A and its main partner B, then what to collect."""
    a = advice - 1
    row = confusion[a]
    b = max((p for p in range(4) if p != a), key=lambda p: (row[p], -p))
    counts = [sum(r) for r in confusion]
    name_a, name_b = ZONE_NAMES[a + 1], ZONE_NAMES[b + 1]
    first = (f'  提案: {_zone(advice)}のデータを追加してください。{name_a} → {name_b} {row[b]} 件、'
             f'{name_b} → {name_a} {confusion[b][a]} 件（件数 {name_a} {counts[a]} 対 {name_b} {counts[b]}）')
    if counts[a] < counts[b]:
        return [first, f'  （件数の少ない部位に境界が不利になるので、まず {name_a} を {name_b} と同程度まで増やす）']
    return [first, '  （件数は足りているので、境界付近の押し方（弱め・位置違い）を増やす）']


def run(args, store):
    """Execute one CLI command; returns the lines to print.

    Every command pauses acquisition first (PAI1 MODE 1): in live mode the board
    streams PNEU1 and its small UART RX buffer drops a packet sent in one go.
    """
    if args.command == 'info':
        with store.paused():
            i = store.info()
        return ([f'firmware: {i["firmware"] or "unknown (older build)"}', f'active slot: {i["active"]}']
                + [f'slot {n}: {c} records' for n, c in enumerate(i['counts'])])
    if args.command == 'selftest':
        with store.paused():
            result = store.selftest()
        return [f'FeRAM self test: {"PASS" if result == 0 else f"FAIL ({result})"}']
    if args.command == 'load':
        training = json.loads(args.input.read_text(encoding='utf-8'))
        result = load(store, args.slot, training, args.k, args.name, args.use)
        return [f'slot {args.slot}: {len(training["events"])} records loaded and verified',
                describe_training(result)]
    if args.command == 'dump':
        data = dump(store, args.slot)
        args.out.write_text(json.dumps(data, indent=1), encoding='utf-8')
        return [f'slot {args.slot}: {len(data["events"])} records -> {args.out}']
    with store.paused():
        if args.command == 'use':
            store.use(args.slot)
            return [f'active slot: {args.slot} (retrained when live mode resumes)']
        if args.command == 'erase':
            store.erase(args.slot)
            return [f'slot {args.slot} erased']
        if args.command == 'tune':
            # TUNE leaves instance 0 untrained: TRAIN at once so the report ends with the tuned model.
            lines = describe_tune(store.tune())
            return lines + [describe_training(store.train())]
        return [describe_training(store.train())]


def parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--port', required=True)
    p.add_argument('--log', type=Path, default=None, help='UART transcript (TX/RX lines)')
    sub = p.add_subparsers(dest='command', required=True)
    for name in ('info', 'train', 'tune', 'selftest'):
        sub.add_parser(name)
    for name in ('use', 'erase'):
        sub.add_parser(name).add_argument('--slot', type=int, required=True)
    load_p = sub.add_parser('load')
    load_p.add_argument('--slot', type=int, required=True)
    load_p.add_argument('--input', type=Path, required=True)
    load_p.add_argument('--k', type=int, default=0, help='0 = chip picks automatically, 1..12 = best k')
    load_p.add_argument('--name', default=date.today().strftime('%Y%m%d'))
    load_p.add_argument('--use', action='store_true', help='make this slot active')
    dump_p = sub.add_parser('dump')
    dump_p.add_argument('--slot', type=int, required=True)
    dump_p.add_argument('--out', type=Path, required=True)
    return p


def main(argv=None, device_factory=None):
    args = parser().parse_args(argv)
    if device_factory is None:
        from validate_solist_ai import Device

        def device_factory(port, log):
            return Device(port, log)
    with contextlib.ExitStack() as stack:
        log = stack.enter_context(open(args.log or os.devnull, 'w', encoding='utf-8', buffering=1))
        device = device_factory(args.port, log)
        if hasattr(device, 'serial'):
            stack.callback(device.serial.close)
        try:
            lines = run(args, Store(device))
        except (ValueError, RuntimeError, TimeoutError, OSError) as error:
            raise SystemExit(f'model_store: {error}')
    print('\n'.join(lines))
    return lines


if __name__ == '__main__':
    main()
