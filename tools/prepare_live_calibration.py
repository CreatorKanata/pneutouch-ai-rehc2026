"""Extract labelled live recordings with the exact firmware C implementation.

Initial partial gestures and events without matching firmware telemetry are
excluded. Class labels come from the capture instruction/user confirmation,
never from the MCU's predicted class.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'docs/analysis/2026-09-20-live-calibration'
SOURCES = [
    ('pressure-head-20260920-200652.csv',4),
    ('pressure-head-supplement-20260920.csv',4),
    ('pressure-back-20260920.csv',2),
    ('pressure-legs-20260920.csv',3),
    ('pressure-tail-20260920.csv',1),
]


def prepare(executable):
    old_path = ROOT/'docs/analysis/2026-09-20-pressure/results/solist-inputs.json'
    old = json.loads(old_path.read_text(encoding='utf8'))
    events = [dict(e, source_set='original') for e in old['events']]
    records = []
    for filename, label in SOURCES:
        path = BASE/filename
        meta = json.loads(path.with_suffix('.json').read_text(encoding='utf8'))
        assert meta['stop_reason'] == 'user_end' and meta['sequence_gaps'] == 0
        telemetry = {}
        for line in path.with_suffix('.uart.log').read_text(encoding='utf8').splitlines():
            if line.startswith('# PNEF1,'):
                f = line.split(',')
                key = int(f[2]),int(f[3])
                parts = telemetry.setdefault(key,{})
                parts[int(f[4])] = [float(v)/1000 for v in f[5:]]
        output = subprocess.check_output([str(executable),str(path)], text=True)
        completed = kept = 0
        for line in output.splitlines():
            row = [float(v) for v in line.split(',')]
            if row[0] != 2:
                continue
            completed += 1
            trigger, end = int(row[2]),int(row[3])
            parts = telemetry.get((trigger,end),{})
            if set(parts) != {0,6}:
                continue
            features = row[4:16]
            measured = parts[0]+parts[6]
            # Telemetry is decimal features x1000, rounded by the MCU.
            assert np.allclose(features, measured, rtol=1e-5, atol=.001), filename
            events.append(dict(file=filename,event=int(row[1]),label=label,
                source_set='live_calibration',trigger_ms=trigger,end_ms=end,
                features12=features,baseline=row[16],peak=row[17],trough=row[18]))
            kept += 1
        records.append(dict(file=filename,label=label,complete_replay=completed,
            matched_firmware=kept,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    result = dict(feature_order=old['feature_order'],events=events,records=records,
        original_sha256=hashlib.sha256(old_path.read_bytes()).hexdigest(),
        labels='User-specified part for each capture. Supplemental HEAD all confirmed by user.',
        excluded='Partial boundary gestures, rejected/incomplete pairs, missing/mismatched firmware telemetry.')
    dest = BASE/'pressure-demo-inputs.json'
    dest.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(records,indent=2))
    print('Prepared',len(events),'events:',dest)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--replay',type=Path,default=ROOT/'build/replay_pressure_features_cached.exe')
    a = p.parse_args()
    prepare(a.replay.resolve())
