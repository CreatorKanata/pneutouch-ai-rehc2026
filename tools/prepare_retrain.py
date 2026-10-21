"""Turn a folder of live dinosaur recordings into Solist-AI training data.

Each zone is recorded with tools/capture_live_calibration.py, which writes
``<name>.csv`` (host_utc,seq,mcu_ms,raw,label), ``<name>.json`` and
``<name>.uart.log``. The zone comes from the CSV file name's first word:

    head-20261022-1.csv   -> HEAD (label 4)
    back_20261022.csv     -> BACK (label 2)
    legs-1.csv            -> LEGS (label 3)
    tail-1.csv            -> TAIL (label 1)

Features are the firmware's own ``# PNEF1`` telemetry (x1000 integers); no
Python feature extraction is used. The firmware's ``# PNEC1`` class is the OLD
model's guess and is only recorded for comparison, never used as the label.

Run:
    .venv/bin/python tools/prepare_retrain.py captures/dino-85a --out docs/analysis/2026-10-22-retrain

With ``--replay build/replay_pressure_features`` the features, baseline and
peak come from the host build of the firmware extractor run on each CSV
(tests/replay_pressure_features.c) instead of the PNEF1 telemetry; the
.uart.log is then optional and only supplies the old model's prediction.

Writes pressure-demo-training.json (input of tools/generate_pressure_model.py),
events.csv, summary.md and waveforms.png.
"""
import argparse
import csv
import hashlib
import json
import re
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_pressure as ap  # noqa: E402
import retrain_replay as replay  # noqa: E402
import retrain_report as report  # noqa: E402

FEATURE_ORDER = ap.POS_SHAPE + ap.NEG_SHAPE + ap.RATIOS
ZONES = {'tail': 1, 'back': 2, 'legs': 3, 'head': 4}
ZONE_NAMES = {v: k.upper() for k, v in ZONES.items()}
SOURCE_SET = 'dino_85a_live'
BASELINE_WINDOW_MS = (800, 300)
BASELINE_MIN_SAMPLES = 5
BASELINE_FALLBACK_SAMPLES = 20
MAX_EVENTS = 255
MODEL_SEED, MODEL_EPOCHS = 1, 4
TRAINING_NAME = 'pressure-demo-training.json'


def zone_of(path):
    """Label from the stem's first word (before '-' or '_'), or None."""
    word = re.split(r'[-_]', Path(path).stem, maxsplit=1)[0].lower()
    return ZONES.get(word)


def _ints(fields):
    return [int(v) for v in fields]


def _new_telemetry():
    # Event ids restart after an MCU reset, so feature entries are keyed by
    # (id, trigger_ms) and START/INVALID/TIMEOUT are kept as ordered lists.
    return dict(started=[], invalid=[], timeout=[], features={}, predicted={})


def parse_line(tele, line):
    """Fold one UART line into the telemetry accumulator; ignore anything else."""
    at = line.find('# PNE')
    if at < 0:
        return
    f = line[at + 2:].strip().split(',')
    try:
        if f[0] == 'PNEE1' and len(f) == 4:
            kind = {'START': 'started', 'INVALID': 'invalid', 'TIMEOUT': 'timeout'}.get(f[2])
            if kind:
                tele[kind].append((int(f[1]), int(f[3])))
        elif f[0] == 'PNEF1' and len(f) == 11:
            eid, trigger, end, start = _ints(f[1:5])
            if start in (0, 6):
                # PNEC1 precedes the PNEF1 pair of the same event in the firmware.
                entry = tele['features'].setdefault((eid, trigger), dict(
                    trigger_ms=trigger, end_ms=end, parts={}, predicted=tele['predicted'].get(eid)))
                entry['parts'][start] = [v / 1000 for v in _ints(f[5:])]
        elif f[0] == 'PNEC1' and len(f) >= 4:
            tele['predicted'][int(f[1])] = f[3]
    except ValueError:
        return  # garbled / interleaved line


def parse_telemetry(text):
    tele = _new_telemetry()
    for line in text.splitlines():
        parse_line(tele, line)
    return tele


def load_samples(path):
    """(mcu_ms, raw) arrays; tolerant of empty files and malformed rows."""
    ms, raw = [], []
    with Path(path).open(encoding='utf-8', newline='') as f:
        for row in csv.DictReader(f):
            try:
                ms.append(int(row['mcu_ms'])); raw.append(float(row['raw']))
            except (KeyError, TypeError, ValueError):
                continue
    return np.array(ms, dtype=np.int64), np.array(raw, dtype=float)


def baseline_and_peak(ms, raw, trigger, end):
    """Baseline before the trigger and positive peak in [trigger, end]; None if no samples."""
    window = (ms >= trigger) & (ms <= end)
    if not window.any():
        return None
    lo, hi = trigger - BASELINE_WINDOW_MS[0], trigger - BASELINE_WINDOW_MS[1]
    pre = raw[(ms >= lo) & (ms <= hi)]
    if len(pre) < BASELINE_MIN_SAMPLES:
        pre = raw[ms < trigger][-BASELINE_FALLBACK_SAMPLES:]
    if not len(pre):
        return None
    baseline = float(np.median(pre))
    return baseline, float(raw[window].max() - baseline)


def build_event(name, label, eid, entry, samples):
    features = entry['parts'].get(0, []) + entry['parts'].get(6, [])
    event = dict(file=name, event=eid, label=label, zone=ZONE_NAMES[label], source_set=SOURCE_SET,
                 trigger_ms=entry['trigger_ms'], end_ms=entry['end_ms'], features12=features,
                 peak=None, baseline=None, firmware_predicted=entry['predicted'])
    if set(entry['parts']) != {0, 6}:
        return dict(event, reason='incomplete_feature_parts')
    if any(v <= 0 for v in features):
        return dict(event, reason='nonpositive_feature')
    bp = baseline_and_peak(*samples, entry['trigger_ms'], entry['end_ms'])
    if bp is None:
        return dict(event, reason='no_samples_in_window')
    event = dict(event, baseline=bp[0], peak=bp[1])
    return dict(event, reason='' if bp[1] > 0 else 'nonpositive_peak')


def rejected_events(name, label, tele):
    started = dict(tele['started'])
    return [dict(file=name, event=eid, label=label, zone=ZONE_NAMES[label], source_set=SOURCE_SET,
                 trigger_ms=started.get(eid), end_ms=ms, features12=[], peak=None, baseline=None,
                 firmware_predicted=None, reason=kind)
            for kind in ('invalid', 'timeout') for eid, ms in tele[kind]]


def read_telemetry(path):
    log = path.with_suffix('.uart.log')
    return parse_telemetry(log.read_text(encoding='utf-8', errors='replace')) if log.exists() else _new_telemetry()


def make_record(path, label, counts, events):
    return dict(file=path.name, label=label, zone=ZONE_NAMES[label], **counts,
                kept=sum(not e['reason'] for e in events),
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def telemetry_file(path, label):
    """Events from the firmware's PNEF1 telemetry; peak/baseline from the CSV."""
    tele = read_telemetry(path)
    samples = load_samples(path)
    events = [build_event(path.name, label, eid, entry, samples)
              for (eid, _), entry in tele['features'].items()]
    events += rejected_events(path.name, label, tele)
    counts = dict(started=len(tele['started']), completed=len(tele['features']),
                  invalid=len(tele['invalid']), timeout=len(tele['timeout']))
    return events, make_record(path, label, counts, events)


def replay_event(name, label, row, predictions):
    event = dict(file=name, event=row['event'], label=label, zone=ZONE_NAMES[label], source_set=SOURCE_SET,
                 trigger_ms=row['trigger_ms'], end_ms=row['ms'], features12=row.get('features12', []),
                 peak=row.get('peak'), baseline=row.get('baseline'),
                 firmware_predicted=predictions.get(row['trigger_ms']))
    if row['kind'] != 'complete':
        return dict(event, reason=row['kind'])
    if any(v <= 0 for v in event['features12']):
        return dict(event, reason='nonpositive_feature')
    return dict(event, reason='' if event['peak'] > 0 else 'nonpositive_peak')


def replay_file(path, label, exe):
    """Events from the host replay; the .uart.log (optional) only gives the old prediction."""
    rows = replay.run_replay(exe, path)
    predictions = {e['trigger_ms']: e['predicted'] for e in read_telemetry(path)['features'].values()}
    events = [replay_event(path.name, label, r, predictions) for r in rows if r['kind'] != 'start']
    counts = {k: sum(r['kind'] == kind for r in rows) for k, kind in
              (('started', 'start'), ('completed', 'complete'), ('invalid', 'invalid'), ('timeout', 'timeout'))}
    return events, make_record(path, label, counts, events)


def scan(folder, exe=None):
    """Process every CSV in the folder; returns events, records and skipped files."""
    events, records, skipped = [], [], []
    for path in sorted(Path(folder).glob('*.csv')):
        label = zone_of(path)
        if label is None:
            skipped.append(dict(file=path.name, reason='file name does not start with head/back/legs/tail'))
        elif exe is None and not path.with_suffix('.uart.log').exists():
            skipped.append(dict(file=path.name, reason='no .uart.log (firmware features required)'))
        else:
            file_events, record = replay_file(path, label, exe) if exe else telemetry_file(path, label)
            events += file_events
            records.append(record)
    return events, records, skipped


def source_note(source):
    if source['kind'] == 'replay':
        return (f"Features, baseline and peak from the host replay {source['exe']} "
                f"(sha256 {source['sha256']}) of pressure_features.c on each CSV; end_ms is the sample "
                'time at which the replay completed the event.')
    return ("Features are the firmware's PNEF1 telemetry /1000; peak = max raw in [trigger,end] minus "
            'the median raw in [trigger-800, trigger-300] ms.')


def training_json(kept, records, hidden, source):
    keys = ('file', 'event', 'label', 'zone', 'source_set', 'trigger_ms', 'end_ms',
            'features12', 'peak', 'baseline', 'firmware_predicted')
    return dict(feature_order=FEATURE_ORDER, events=[{k: e[k] for k in keys} for e in kept],
                records=records, model=dict(hidden=hidden, seed=MODEL_SEED, epochs=MODEL_EPOCHS),
                source=source,
                note='Labels from the capture file name (zone pressed by the user). ' + source_note(source)
                     + ' firmware_predicted is the OLD model (PNEC1), not a label.')


def write_events_csv(path, events):
    keys = ['zone', 'label', 'file', 'event', 'trigger_ms', 'end_ms', 'kept', 'reason',
            'firmware_predicted', 'peak', 'baseline'] + FEATURE_ORDER
    with path.open('w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for e in events:
            feats = dict(zip(FEATURE_ORDER, e['features12']))
            w.writerow({**{k: e.get(k) for k in keys if k not in feats}, 'kept': not e['reason'], **feats})


def run(folder, out, hidden=32, min_events=10, exe=None):
    """Build every output in ``out``; returns the summary context."""
    if hidden not in (32, 64):
        raise ValueError('--hidden must be 32 or 64')
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    source = replay.describe(exe) if exe else dict(kind='telemetry')
    events, records, skipped = scan(folder, exe and source['exe'])
    kept = [e for e in events if not e['reason']]
    (out / TRAINING_NAME).write_text(
        json.dumps(training_json(kept, records, hidden, source), ensure_ascii=False, indent=2), encoding='utf-8')
    write_events_csv(out / 'events.csv', events)
    context = dict(out=out, events=events, kept=kept, records=records, skipped=skipped,
                   min_events=min_events, max_events=MAX_EVENTS, zone_names=ZONE_NAMES,
                   feature_order=FEATURE_ORDER, training=out / TRAINING_NAME,
                   source=source, source_note=source_note(source))
    report.write_summary(context)
    report.plot_waveforms(kept, Path(folder), ZONE_NAMES, out / 'waveforms.png', load_samples)
    return context


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--hidden', type=int, choices=(32, 64), default=32)
    parser.add_argument('--min-events', type=int, default=10)
    parser.add_argument('--replay', type=Path, default=None, metavar='EXE',
                        help='host build of tests/replay_pressure_features.c; features from replay, not PNEF1')
    args = parser.parse_args(argv)
    if not args.folder.is_dir():
        parser.error(f'{args.folder} is not a folder')
    if args.replay is not None and not args.replay.is_file():
        parser.error(f'{args.replay} is not a file')
    context = run(args.folder, args.out, args.hidden, args.min_events, args.replay)
    print((context['out'] / 'summary.md').read_text(encoding='utf-8'))
    return context


if __name__ == '__main__':
    main()
