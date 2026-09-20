"""Exploratory physical-chip comparison after the HEAD -> LEGS live report.

Reuse labelled captures with per-class contiguous four-fold validation.
This is development evidence, not a new independent/untouched test set.
Keep 12 inputs, seed 1 and four epochs; compare 32/64 units and input scale.
Replace the redundant decay/rise feature with absolute positive/negative peak.
No captures or feature schema in the deployed firmware are modified here.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time

import numpy as np
from validate_live_model import predict
from validate_solist_ai import Device

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/analysis/2026-09-20-live-calibration'


def fit_variant(device, values, labels, indices, hidden, divisor):
    logged = np.log(values[indices].astype(np.float64))
    mean = logged.mean(0).astype(np.float32)
    scale = (divisor * logged.std(0)).astype(np.float32)
    assert (scale > 1e-8).all()
    z = (np.log(values) - mean) / scale
    device.initialize(12, hidden, 1, 1)
    rng = np.random.default_rng(1)
    for _ in range(4):
        for i in rng.permutation(indices):
            device.sample(z[i], labels[i])
    return z, dict(mean=mean.tolist(), scale=scale.tolist())


def run(port='COM7', hidden=32, divisor=3, specialist=False):
    source = BASE / 'pressure-demo-training.json'
    data = json.loads(source.read_text(encoding='utf8'))
    events = data['events']
    x = np.array([e['features12'] for e in events], dtype=np.float32)
    y = np.array([e['label'] for e in events])
    ids = np.arange(len(y))
    variants = {'shape12': x}
    for name, field in [('positive_peak12', 'peak'), ('negative_peak12', 'trough')]:
        v = x.copy()
        v[:, 5] = np.array([e[field] / 1000 for e in events], dtype=np.float32)
        variants[name] = v
    folds = [np.concatenate([np.array_split(ids[y == c], 4)[f] for c in (1, 2, 3, 4)])
             for f in range(4)]
    report = dict(utc=datetime.now(timezone.utc).isoformat(),
                  source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  meaning='Exploratory re-use of previously examined same-session data; not independent accuracy.',
                  backend='physical ML63Q2557 Solist-AI', port=port,
                  settings=dict(inputs=12, hidden=hidden, seed=1, epochs=4,
                                normalization=f'log / {divisor} training standard deviations'),
                  folds=[f.tolist() for f in folds], variants={})
    suffix = '' if hidden == 32 else f'-h{hidden}'
    if divisor != 3:
        suffix += f'-s{divisor}'
    if specialist:
        suffix += '-specialist'
        baseline = json.loads((BASE / 'head-legs-feature-comparison.json').read_text(encoding='utf8'))
        baseline = baseline['variants']['shape12']
        report['specialist'] = 'Apply only when frozen baseline predicts HEAD or LEGS; train only these classes.'
    dest = BASE / f'head-legs-feature-comparison{suffix}.json'

    def save():
        dest.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf8')

    with dest.with_suffix('.uart.log').open('w', encoding='utf8', buffering=1) as log:
        device = Device(port, log)
        try:
            device.command('MODE', 1)
            for name, values in variants.items():
                results = []
                for fold, test in enumerate(folds):
                    train = np.setdiff1d(ids, test)
                    if specialist:
                        train = train[(y[train] == 3) | (y[train] == 4)]
                    z, pre = fit_variant(device, values, y, train, hidden, divisor)
                    result = predict(device, z, y, test)
                    if specialist:
                        result['specialist_scores'] = result['scores']
                        scores = np.array(result['scores'])
                        base = np.array(baseline['folds'][fold]['predictions'])
                        pred = np.where((base == 3) | (base == 4), scores[:, 2:4].argmax(1) + 3, base)
                        cm = [[int(((y[test] == a) & (pred == b)).sum()) for b in (1, 2, 3, 4)] for a in (1, 2, 3, 4)]
                        result.update(predictions=pred.tolist(), confusion_matrix=cm,
                                      correct=int((pred == y[test]).sum()))
                        result['balanced_accuracy'] = float(np.mean([np.mean(pred[y[test] == c] == c) for c in (1, 2, 3, 4)]))
                    result.update(fit_indices=train.tolist(), preprocessing=pre)
                    results.append(result)
                    print(name, 'fold', fold + 1, result['correct'], '/', result['total'], flush=True)
                cm = np.sum([r['confusion_matrix'] for r in results], axis=0)
                full_fit = ids[(y == 3) | (y == 4)] if specialist else ids
                z, pre = fit_variant(device, values, y, full_fit, hidden, divisor)
                reconstruction = predict(device, z, y, full_fit)
                if specialist:
                    target = y[full_fit]
                    pred = np.argmax(np.array(reconstruction['scores'])[:, 2:4], axis=1) + 3
                    reconstruction.update(predictions=pred.tolist(), correct=int((pred == target).sum()),
                        confusion_matrix=[[int(((target == a) & (pred == b)).sum()) for b in (1, 2, 3, 4)] for a in (1, 2, 3, 4)],
                        balanced_accuracy=float(np.mean([np.mean(pred[target == c] == c) for c in (3, 4)])))
                report['variants'][name] = dict(folds=results, confusion_matrix=cm.tolist(),
                    correct=int(np.trace(cm)), total=len(ids),
                    per_class_recall=(np.diag(cm) / cm.sum(axis=1)).tolist(),
                    reconstruction=reconstruction, final_preprocessing=pre)
                save()
                print(name, 'combined', int(np.trace(cm)), '/', len(ids), 'matrix', cm.tolist(), flush=True)
            report['status'] = 'complete'
        finally:
            try:
                device.command('MODE', 0)
                report['acquisition_resumed'] = True
                time.sleep(.5)
            except Exception as error:
                report['acquisition_resumed'] = False
                report['resume_error'] = str(error)
                raise
            finally:
                device.serial.close()
                save()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM7')
    parser.add_argument('--hidden', type=int, choices=(32, 64), default=32)
    parser.add_argument('--scale-divisor', type=int, choices=(1, 3), default=3)
    parser.add_argument('--specialist', action='store_true')
    args = parser.parse_args()
    run(args.port, args.hidden, args.scale_divisor, args.specialist)
