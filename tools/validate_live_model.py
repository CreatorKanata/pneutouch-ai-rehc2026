"""Select and check a 12-feature model using labelled current physical captures.

Each class's last two live events are held out until AFTER selection. The two
preceding events select between 32/64 hidden units and reference+live/live-only
training. Seed=1 and epochs=4 are fixed. All arithmetic runs on the real chip.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
import numpy as np
from validate_solist_ai import Device

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'docs/analysis/2026-09-20-live-calibration'


def normalize(x, fit):
    logged = np.log(x[fit].astype(np.float64))
    mean, scale = logged.mean(0).astype(np.float32), (3*logged.std(0)).astype(np.float32)
    assert (scale > 1e-8).all()
    return (np.log(x)-mean)/scale, dict(mean=mean.tolist(),scale=scale.tolist())


def fit_model(device, x, y, fit, hidden):
    z, preprocessing = normalize(x,fit)
    device.initialize(12,hidden,1,1)
    rng = np.random.default_rng(1)
    for _ in range(4):
        for i in rng.permutation(fit):
            device.sample(z[i],y[i])
    return z, preprocessing


def predict(device, z, y, indices):
    scores = np.array([device.sample(z[i])[0] for i in indices])
    pred = scores.argmax(1)+1
    target = y[indices]
    cm = [[int(((target == a)&(pred == b)).sum()) for b in range(1,5)] for a in range(1,5)]
    ba = float(np.mean([np.mean(pred[target == label] == label) for label in np.unique(target)]))
    return dict(indices=indices.tolist(),targets=target.tolist(),predictions=pred.tolist(),
        scores=scores.tolist(),correct=int((pred == target).sum()),total=len(indices),
        balanced_accuracy=ba,confusion_matrix=cm)


def run(port):
    source = json.loads((BASE/'pressure-demo-inputs.json').read_text(encoding='utf8'))
    events = source['events']
    x = np.array([e['features12'] for e in events],dtype=np.float32)
    y = np.array([e['label'] for e in events])
    original = np.array([i for i,e in enumerate(events) if e['source_set'] == 'original'])
    live = np.array([i for i,e in enumerate(events) if e['source_set'] == 'live_calibration'])
    val, test = [], []
    for label in range(1,5):
        indices = live[y[live] == label]
        assert len(indices) >= 6, f'Need at least six valid current examples for class {label}'
        val.extend(indices[-4:-2]); test.extend(indices[-2:])
    val, test = np.array(val),np.array(test)
    report = dict(utc=datetime.now(timezone.utc).isoformat(),port=port,
        selection_indices=val.tolist(),heldout_indices=test.tolist(),candidates=[],
        evaluation='Within-recording chronological check. No independently recorded/operator test.',
        settings=dict(inputs=12,seed=1,epochs=4,activation='hard sigmoid',beta=0,p='identity'))
    dest = BASE/'model-selection.json'
    def save(): dest.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    with dest.with_suffix('.uart.log').open('w',encoding='utf8',buffering=1) as log:
        d = Device(port,log)
        try:
            d.command('MODE',1)
            for hidden in (32,64):
                for include_original in (True,False):
                    pool = np.arange(len(events)) if include_original else live
                    fit = np.setdiff1d(pool,np.concatenate([val,test]))
                    z, pre = fit_model(d,x,y,fit,hidden)
                    result = dict(hidden=hidden,include_original=include_original,
                        fit_indices=fit.tolist(),preprocessing=pre,selection=predict(d,z,y,val))
                    report['candidates'].append(result); save()
                    print('Selection',hidden,'reference+live' if include_original else 'live only',
                          result['selection']['correct'],'/',len(val),flush=True)
            best = max(report['candidates'],key=lambda r:r['selection']['balanced_accuracy'])
            report['chosen'] = dict(hidden=best['hidden'],include_original=best['include_original'])
            pool = np.arange(len(events)) if best['include_original'] else live
            fit = np.setdiff1d(pool,test)
            z, pre = fit_model(d,x,y,fit,best['hidden'])
            report['heldout_fit_indices'] = fit.tolist()
            report['heldout_preprocessing'] = pre
            report['heldout'] = predict(d,z,y,test); save()
            print('Untouched holdout:',report['heldout']['correct'],'/',len(test),flush=True)
            # Baseline check is descriptive and is not used to select a model.
            z, _ = fit_model(d,x,y,original,32)
            report['original_model_on_same_holdout'] = predict(d,z,y,test); save()
            final = dict(feature_order=source['feature_order'],events=[events[i] for i in pool],
                model=dict(hidden=best['hidden'],seed=1,epochs=4),
                note='Deployment refit includes all selected examples, including prior heldout rows. Heldout result belongs to heldout_fit_indices, not this final refit.')
            (BASE/'pressure-demo-training.json').write_text(json.dumps(final,ensure_ascii=False,indent=2),encoding='utf8')
            report['final_fit_count'] = len(pool)
            report['status'] = 'complete'; save()
        finally:
            d.command('MODE',0); time.sleep(.5); d.serial.close()


if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('--port',default='COM7')
    run(p.parse_args().port)
