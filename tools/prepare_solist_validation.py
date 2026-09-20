"""Replay the firmware C extractor and prepare causally aligned chip inputs.

Build the harness first (CC may be 'zig cc'):
  cc -std=c11 -Isrc/pneutouch-solist/S_PneuTouch tests/replay_pressure_features.c \
     src/pneutouch-solist/S_PneuTouch/pressure_features.c -o build/replay_pressure_features
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
from analyze_pressure import DEFAULT, FAMILIES, load


def prepare(executable):
    manifest = json.loads((DEFAULT/'manifest.json').read_text(encoding='utf8'))
    previous = json.loads((DEFAULT/'results/causal-replay.json').read_text(encoding='utf8'))
    records, events, errors = [], [], []
    for meta, reference in zip(manifest['captures'], previous['records']):
        path = DEFAULT/meta['file']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == meta['sha256']
        assert meta['file'] == reference['file']
        _, _, ms, t, raw = load(path)
        output = subprocess.check_output([str(executable), str(path)], text=True)
        rows = [[float(x) for x in line.split(',')] for line in output.splitlines()]
        complete = [r for r in rows if r[0] == 2]
        assert len(complete) == reference['matched']
        assert all(r[0] in (1, 2) for r in rows), 'Rejected/timeout event in replay'
        comparisons = []
        for r, ref in zip(complete, reference['events']):
            trigger, end = (r[2]-ms[0])/1000, (r[3]-ms[0])/1000
            assert abs(trigger-ref['trigger_s']) < 1e-6
            assert abs(end-ref['end_s']) < 1e-6
            features = np.array(r[4:16])
            old = np.array([ref['completed_features'][f] for f in FAMILIES['shape_ratios']])
            relative = np.abs(features-old)/old
            errors.append(relative)
            comparisons.append(dict(id=int(r[1]), trigger_s=trigger, end_s=end,
                                    feature_relative_error=relative.tolist()))
            if meta['kind'] != 'seven_zone':
                continue
            # Baseline is observed BEFORE trigger; amplitude normalization uses
            # only the fixed window. No true onset, peak alignment or duration warp.
            prior = raw[(t >= trigger-.825) & (t <= trigger-.325)]
            baseline = float(np.median(prior))
            waves = {}
            for length in (64, 128):
                grid = trigger-.2+np.arange(length)*.025
                assert grid[0] >= t[0] and grid[-1] <= t[-1]
                v = np.interp(grid, t, raw)-baseline
                waves[f'wave{length}'] = (v/max(float(np.max(np.abs(v))), 1)).tolist()
            zone = meta['zone_id']
            events.append(dict(file=path.name, event=int(r[1]), zone=zone,
                label=1 if zone == 1 else 2 if zone == 2 else 4 if zone == 7 else 3,
                trigger_s=trigger, completed_after_s=end-trigger,
                features12=features.tolist(), **waves))
        records.append(dict(file=path.name, starts=sum(r[0] == 1 for r in rows),
                            complete=len(complete), comparisons=comparisons))
    assert len(events) == 43
    # The bounded extractor's local baseline differs slightly from the batch
    # reference, but the recorded pilot vectors must remain within one percent.
    assert np.max(errors) < .01
    report = dict(feature_order=FAMILIES['shape_ratios'], events=events, records=records,
        max_relative_feature_error=np.max(errors, axis=0).tolist(),
        waveform=dict(interval_ms=25, start_relative_trigger_ms=-200,
                      wave64_ready_ms=1375, wave128_ready_ms=2975,
                      baseline='median from trigger-825ms through trigger-325ms',
                      normalization='divide by maximum absolute value within each window'),
        limitations=['One recording per original zone; within-record splits are exploratory.',
                     'Wave128 observes longer than features12 or wave64.'])
    dest = DEFAULT/'results/solist-inputs.json'
    dest.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf8')
    print('Replay:', [(r['file'], r['complete']) for r in records])
    print('Max relative feature errors:', report['max_relative_feature_error'])
    print('Prepared', len(events), 'events at', dest)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--replay', type=Path, default=Path('build/replay_pressure_features.exe'))
    args = parser.parse_args()
    prepare(args.replay.resolve())
