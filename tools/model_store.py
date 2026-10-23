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
    .venv/bin/python tools/model_store.py --port COM7 selftest
"""
import argparse
import contextlib
import json
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
        return dict(active=int(r[0]), counts=[int(v) for v in r[1:5]])

    def slot(self, slot):
        r = self._cmd('SLOT', check_slot(slot))
        return dict(slot=int(r[0]), state=int(r[1]), count=int(r[2]), pool=int(r[3]), k=int(r[4]), name=r[5])

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
    info = store.slot(slot)
    events = []
    with store.paused():
        for index in range(info['count'] if info['state'] else 0):
            label, row = store.get(slot, index)
            events.append(dict(label=label, zone=ZONE_NAMES[label], features12=[float(v) for v in row[:12]],
                               peak=float(row[12]), trough=float(row[13])))
    return dict(feature_order=FEATURE_ORDER, events=events,
                source=dict(kind='fram', slot=slot, name=info['name'], k=info['k'], state=STATES[info['state']]))


def describe_training(result):
    names = ', '.join(fs.FEATURE_NAMES[j] for j in result['selected']) or 'none'
    status = 'READY' if result['ready'] else f'ERROR {result["error"]}'
    return (f'{status}: {result["count"]} records, {result["k"]} features selected ({names}), '
            f'training {result["training_us"] / 1e6:.2f} s')


def run(args, store):
    """Execute one CLI command; returns the lines to print."""
    if args.command == 'info':
        i = store.info()
        return [f'active slot: {i["active"]}'] + [f'slot {n}: {c} records' for n, c in enumerate(i['counts'])]
    if args.command == 'selftest':
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
        return [describe_training(store.train())]


def parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--port', required=True)
    p.add_argument('--log', type=Path, default=None, help='UART transcript (TX/RX lines)')
    sub = p.add_subparsers(dest='command', required=True)
    for name in ('info', 'train', 'selftest'):
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
