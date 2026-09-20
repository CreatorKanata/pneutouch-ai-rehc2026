"""Reproduce the 2026-09-20 pressure study. No device access or model deployment.

Run: python tools/analyze_pressure.py
Inputs, hashes and generated evidence live in docs/analysis/2026-09-20-pressure.
All predictive scores are exploratory within-session checks, not field accuracy.
"""
import argparse
import csv
import hashlib
import json
import platform
from pathlib import Path
from collections import Counter

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'docs/analysis/2026-09-20-pressure'
NAMES = ['しっぽ', '背中', '右後ろ足', '左後ろ足', '右前足', '左前足', '首と頭']
POS_SHAPE = ['rise_ms', 'decay_ms', 'width_ms', 'area_width_ms', 'slope_norm_s', 'asymmetry']
NEG_SHAPE = ['release_rise_ms', 'recovery_ms', 'release_width_ms', 'release_area_width_ms']
RATIOS = ['trough_peak_ratio', 'area_ratio']
AMPLITUDE = ['peak_counts', 'trough_counts']
FAMILIES = {
    'amplitude': AMPLITUDE,
    'press_shape': POS_SHAPE,
    'release_shape': NEG_SHAPE,
    'shape_ratios': POS_SHAPE + NEG_SHAPE + RATIOS,
    'full_features': AMPLITUDE + POS_SHAPE + NEG_SHAPE + RATIOS,
    'spectral_shape': ['spectral_centroid_hz', 'spectral_rms_hz', 'spectral_high_fraction'],
    'relative_integrals': ['area_width_ms', 'release_area_width_ms', 'trough_peak_ratio', 'area_ratio', 'slope_norm_s'],
    'compact_time_ratios': ['rise_ms', 'decay_ms', 'release_width_ms', 'trough_peak_ratio', 'area_ratio'],
    'original_press_features': ['peak_counts', 'rise_ms', 'peak_from_wave_onset_ms', 'decay_ms', 'area_counts_s', 'slope_norm_s'],
    'baseline_control': ['baseline_counts'],
}


def load(path):
    with path.open(encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f))
    seq = np.array([int(r['seq']) for r in rows], dtype=np.int64)
    ms = np.array([int(r['mcu_ms']) for r in rows], dtype=np.int64)
    raw = np.array([int(r['raw']) for r in rows], dtype=float)
    t = np.r_[0, np.cumsum(np.diff(ms) % 2**32)] / 1000
    assert len(rows) > 1 and np.all(np.diff(t) > 0)
    return rows, seq, ms, t, raw


def detect(t, y, threshold=50000):
    """One shared rule for every zone; merge shoulders, never enforce a count."""
    baseline = float(np.median(y))
    on = y > baseline + threshold
    starts = np.flatnonzero(np.diff(on.astype(int), prepend=0) == 1)
    ends = np.flatnonzero(np.diff(on.astype(int), append=0) == -1)
    runs = []
    for a, b in zip(starts, ends):
        if runs and t[a] - t[runs[-1][1]] < .4:
            runs[-1][1] = int(b)
        else:
            runs.append([int(a), int(b)])
    return [(a, b, int(a + np.argmax(y[a:b+1]))) for a, b in runs
            if np.max(y[a:b+1]) - baseline >= 100000 and b - a >= 1]


def crossing(t, z, level, lo, hi, up, last=False):
    ix = np.arange(max(0, lo), min(hi, len(t)-1))
    keep = ((z[ix] < level) & (z[ix+1] >= level)) if up else ((z[ix] > level) & (z[ix+1] <= level))
    found = ix[keep]
    if not len(found):
        return None
    k = int(found[-1] if last else found[0])
    return float(t[k] + (level-z[k])/(z[k+1]-z[k])*(t[k+1]-t[k]))


def integral(t, z, start, end):
    inner = (t > start) & (t < end)
    tt = np.r_[start, t[inner], end]
    return float(np.trapezoid(np.interp(tt, t, z), tt))


def decay_fit(t, z, start, end):
    keep = (t >= start) & (t <= end) & (z > 0)
    if np.count_nonzero(keep) < 4:
        return None, None
    xx = t[keep] - start
    yy = np.log(z[keep])
    slope, intercept = np.polyfit(xx, yy, 1)
    pred = intercept + slope*xx
    r2 = 1 - np.sum((yy-pred)**2) / np.sum((yy-np.mean(yy))**2)
    return (float(-1000/slope) if slope < 0 else None), float(r2)


def extract(meta, t, y, threshold=50000):
    runs = detect(t, y, threshold)
    events = []
    for i, (a, b, p) in enumerate(runs):
        zone = meta['zone_id'] if meta['kind'] == 'seven_zone' else (1 if i % 2 == 0 else 2)
        e = dict(file=Path(meta['file']).name, kind=meta['kind'], event=i+1, zone_id=zone,
                 complete=False, peak_s=float(t[p]), peak_index=p,
                 label_basis=meta['label_basis'])
        events.append(e)
        if a == 0 or b == len(t)-1:
            e['exclusion'] = 'recording_boundary'
            continue
        pre = (t >= t[a]-.8) & (t <= t[a]-.3)
        if np.count_nonzero(pre) < 5:
            e['exclusion'] = 'insufficient_pre_event_baseline'
            continue
        base = float(np.median(y[pre]))
        z = y-base
        A = float(z[p])
        lo = int(np.searchsorted(t, t[p]-1.2))
        end = runs[i+1][0] if i+1 < len(runs) else min(len(t)-1, int(np.searchsorted(t, t[p]+4)))
        n = int(p + np.argmin(z[p:end]))
        B = float(-z[n])
        if B < 30000:
            e['exclusion'] = 'no_resolved_negative_pulse'
            continue
        c = {}
        for f in [.1, .5, .9]:
            c[f'pa{f}'] = crossing(t, z, f*A, lo, p, True, True)
            c[f'pd{f}'] = crossing(t, z, f*A, p, n, False)
            c[f'na{f}'] = crossing(t, -z, f*B, p, n, True, True)
            c[f'nd{f}'] = crossing(t, -z, f*B, n, end, False)
        if any(v is None for v in c.values()):
            e['exclusion'] = 'missing_threshold_crossing'
            continue
        rise = c['pa0.9'] - c['pa0.1']
        decay = c['pd0.1'] - c['pd0.9']
        pa = integral(t, z, c['pa0.1'], c['pd0.1'])
        na = integral(t, -z, c['na0.1'], c['nd0.1'])
        pos_ix = (t >= c['pa0.1']-.025) & (t <= t[p]+.025)
        slope = float(np.max(np.diff(z[pos_ix])/np.diff(t[pos_ix])))
        tau, r2 = decay_fit(t, z/A, c['pd0.9'], c['pd0.1'])
        mid = (t >= c['pd0.1']+.1) & (t <= c['na0.1']-.1)
        middle = float(np.median(z[mid])/A) if np.count_nonzero(mid) >= 3 else None
        post = y[(t >= c['nd0.1']+.3) & (t <= min(c['nd0.1']+.8,t[end]))]
        # Mean-removed, Hann-windowed peak-centered segment. This window can
        # include the beginning of the negative pulse; it is not press-only.
        spectral_grid = np.arange(-.25, .775, .025)
        wave = np.interp(t[p]+spectral_grid, t, z/A)
        power = np.abs(np.fft.rfft((wave-wave.mean()) * np.hanning(len(wave))))**2
        freq = np.fft.rfftfreq(len(wave), .025)
        power[0] = 0
        e.update(complete=True, onset_s=c['pa0.1'], trigger_s=float(t[a]),
            negative_onset_s=c['na0.1'], recovery_s=c['nd0.1'], trough_s=float(t[n]), trough_index=n,
            baseline_counts=base, baseline_sd=float(np.std(y[pre])),
            peak_counts=A, trough_counts=B, trough_peak_ratio=B/A,
            rise_ms=rise*1000, decay_ms=decay*1000,
            peak_from_wave_onset_ms=(t[p]-c['pa0.1'])*1000,
            width_ms=(c['pd0.5']-c['pa0.5'])*1000,
            area_counts_s=pa, area_width_ms=pa/A*1000, slope_norm_s=slope/A,
            asymmetry=decay/rise,
            release_rise_ms=(c['na0.9']-c['na0.1'])*1000,
            recovery_ms=(c['nd0.1']-c['nd0.9'])*1000,
            release_width_ms=(c['nd0.5']-c['na0.5'])*1000,
            release_area_counts_s=na, release_area_width_ms=na/B*1000, area_ratio=na/pa,
            apparent_decay_tau_ms=tau, decay_log_r2=r2,
            middle_fraction=middle, hold_proxy_s=c['na0.1']-c['pa0.1'],
            spectral_centroid_hz=float(np.sum(freq*power)/power.sum()),
            spectral_rms_hz=float(np.sqrt(np.sum(freq**2*power)/power.sum())),
            spectral_high_fraction=float(power[freq >= 8].sum()/power.sum()),
            signal_noise_ratio=A/max(float(np.std(y[pre])), 1),
            post_event_shift_counts=float(np.median(post)-base) if len(post) else None,
            extraction_threshold_counts=threshold)
        assert all(e[k] > 0 and np.isfinite(e[k]) for k in POS_SHAPE+NEG_SHAPE+RATIOS+AMPLITUDE)
    return events


def describe(vals):
    v = np.array([x for x in vals if x is not None], dtype=float)
    if not len(v):
        return dict(n=0, **{k: None for k in ['min','q25','median','q75','max','mean','sd']})
    return dict(n=len(v), min=float(v.min()), q25=float(np.percentile(v,25)), median=float(np.median(v)),
                q75=float(np.percentile(v,75)), max=float(v.max()), mean=float(v.mean()), sd=float(v.std()))


def predict(train, labels, test, model='centroid', standardize=True):
    # Fit scale on this training partition only. Families are fixed for each run,
    # but comparing their scores is exploratory selection without a fresh test set.
    scale = np.std(train, axis=0) if standardize else np.ones(train.shape[1])
    scale = np.where(scale > 1e-10, scale, 1)
    mu = np.mean(train, axis=0)
    a, b = (train-mu)/scale, (test-mu)/scale
    classes = np.unique(labels)
    if model == 'centroid':
        ref = np.stack([a[labels == k].mean(axis=0) for k in classes])
        return classes[np.argmin(np.sum((b[:,None,:]-ref[None,:,:])**2,axis=2),axis=1)]
    return labels[np.argmin(np.sum((b[:,None,:]-a[None,:,:])**2,axis=2),axis=1)]


def score(y, pred, classes):
    cm = np.array([[np.sum((y == a) & (pred == b)) for b in classes] for a in classes],dtype=int)
    recalls = np.diag(cm)/np.maximum(cm.sum(axis=1),1)
    f1 = 2*np.diag(cm)/np.maximum(cm.sum(axis=1)+cm.sum(axis=0),1)
    return dict(n=len(y),correct=int(np.sum(y==pred)),accuracy=float(np.mean(y==pred)),
                balanced_accuracy=float(np.mean(recalls)),macro_f1=float(np.mean(f1)),
                confusion=cm.tolist(),classes=list(map(int,classes)))


def evaluate(x, y, events, standardize=True, model='centroid', strata=None):
    strata = y if strata is None else strata
    ids = np.arange(len(y))
    loo = np.array([predict(x[ids != i],y[ids != i],x[i:i+1],model,standardize)[0] for i in ids])
    # Earliest three complete events per zone train; all later events test.
    tr = np.concatenate([np.flatnonzero(strata==k)[:3] for k in np.unique(strata)])
    te = np.setdiff1d(ids,tr)
    assert not np.intersect1d(tr,te).size and len(tr)+len(te)==len(ids)
    chronological = predict(x[tr],y[tr],x[te],model,standardize)
    # Remove sample-count imbalance: exactly five chronological events per zone.
    five = np.concatenate([np.flatnonzero(strata==k)[:5] for k in np.unique(strata)])
    pred5 = np.array([predict(x[five[five!=i]],y[five[five!=i]],x[i:i+1],model,standardize)[0] for i in five])
    # Reverse holdout checks a single favorable chronological direction.
    rev_tr=np.concatenate([np.flatnonzero(strata==k)[-3:] for k in np.unique(strata)])
    rev_te=np.setdiff1d(ids,rev_tr)
    assert not np.intersect1d(rev_tr,rev_te).size
    rev_pred=predict(x[rev_tr],y[rev_tr],x[rev_te],model,standardize)
    return dict(loo=score(y,loo,np.unique(y)), chronological=score(y[te],chronological,np.unique(y)),
                reverse_chronological=score(y[rev_te],rev_pred,np.unique(y)),
                balanced_first_five=score(y[five],pred5,np.unique(y)),
                loo_predictions=loo.tolist(), chronological_test_indices=te.tolist(),
                chronological_predictions=chronological.tolist())


def matrix(events, features):
    return np.log(np.array([[e[f] for f in features] for e in events], dtype=float))


def wave_matrix(events, captures, mode):
    result=[]
    for e in events:
        t,y=captures[e['file']]
        z=y-e['baseline_counts']
        if mode.startswith('prefix'):
            seconds=float(mode.split('_')[1])
            grid=np.arange(0,seconds+.001,.025)
            v=np.interp(e['trigger_s']+grid,t,z)
            v/=max(np.max(np.abs(v)),1)
        elif mode=='press_peak_normalized':
            v=np.interp(e['peak_s']+np.arange(-.25,.775,.025),t,z)/e['peak_counts']
        else:
            v=np.r_[np.interp(e['peak_s']+np.arange(-.25,.775,.025),t,z)/e['peak_counts'],
                    np.interp(e['trough_s']+np.arange(-.6,.625,.025),t,z)/e['trough_counts']]
        result.append(v)
    return np.array(result)


def run(folder, out):
    out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
    events=[]; quality=[]; captures={}; sensitivity={}
    for meta in manifest['captures']:
        path=folder/meta['file']
        assert hashlib.sha256(path.read_bytes()).hexdigest()==meta['sha256']
        rows,seq,ms,t,y=load(path)
        captures[path.name]=(t,y)
        ee=extract(meta,t,y)
        events.extend(ee)
        quality.append(dict(file=path.name,kind=meta['kind'],samples=len(rows),sps=(len(rows)-1)/t[-1],
            duration_s=float(t[-1]),interval_ms=dict(Counter(map(int,np.diff(ms)%2**32))),
            gaps=int(np.count_nonzero(np.diff(seq)%2**32!=1)),
            adc_endpoints=int(np.count_nonzero((y==-8388608)|(y==8388607))),
            start_utc=rows[0]['host_utc'],end_utc=rows[-1]['host_utc'],
            detected=len(ee),complete=sum(e['complete'] for e in ee)))
        sensitivity[path.name]={str(q):len(detect(t,y,q)) for q in [30000,50000,75000,100000]}
    seven=[e for e in events if e['kind']=='seven_zone' and e['complete']]
    previous=[e for e in events if e['kind']=='alternating' and e['complete']]
    assert len(seven)==43 and len(previous)==16, 'Unexpected event count; inspect before continuing.'
    for z in range(1,8):
        assert sum(e['zone_id']==z for e in seven)>=5
    y=np.array([e['zone_id'] for e in seven])
    stats_keys=AMPLITUDE+POS_SHAPE+NEG_SHAPE+RATIOS+[
        'peak_from_wave_onset_ms','apparent_decay_tau_ms','decay_log_r2','middle_fraction',
        'hold_proxy_s','spectral_centroid_hz','spectral_high_fraction','baseline_counts',
        'baseline_sd','signal_noise_ratio','post_event_shift_counts']
    zone_stats={str(z):{k:describe([e[k] for e in seven if e['zone_id']==z]) for k in stats_keys}
                for z in range(1,8)}
    # Descriptive separation; not a significance test or held-out feature ranking.
    separations=[]
    for k in AMPLITUDE+POS_SHAPE+NEG_SHAPE+RATIOS+['spectral_centroid_hz','apparent_decay_tau_ms']:
        valid=np.array([e[k] is not None for e in seven])
        v=np.log(np.array([e[k] for e in seven if e[k] is not None],dtype=float))
        yy=y[valid]
        total=np.sum((v-v.mean())**2)
        between=sum(np.sum(yy==z)*(v[yy==z].mean()-v.mean())**2 for z in np.unique(yy))
        separations.append(dict(feature=k,log_eta_squared=float(between/total)))
    separations.sort(key=lambda d:d['log_eta_squared'],reverse=True)
    checks={}
    for name,features in FAMILIES.items():
        xx=matrix(seven,features)
        checks[name]={m:evaluate(xx,y,seven,True,m) for m in ['centroid','1nn']}
    wave_modes=['prefix_0.4','prefix_0.8','prefix_1.6','press_peak_normalized','separate_peaks_normalized']
    for mode in wave_modes:
        xx=wave_matrix(seven,captures,mode)
        checks[mode]={m:evaluate(xx,y,seven,False,m) for m in ['centroid','1nn']}
    # A fixed coarse mapping, not learned from these scores: tail/back/legs/head.
    grouped=np.array([1 if z==1 else 2 if z==2 else 4 if z==7 else 3 for z in y])
    four_class={name:{m:evaluate(matrix(seven,features),grouped,seven,True,m,strata=y)
                      for m in ['centroid','1nn']} for name,features in FAMILIES.items() if name!='baseline_control'}
    coarse={name:{m:score(grouped,np.array([1 if z==1 else 2 if z==2 else 4 if z==7 else 3 for z in r[m]['loo_predictions']]),np.arange(1,5))
                  for m in ['centroid','1nn']} for name,r in checks.items()}
    # Cross-recording tail/back only. Earlier labels remain provisional.
    two=[e for e in seven if e['zone_id'] in [1,2]]
    transfers={}
    for name,features in FAMILIES.items():
        if name=='baseline_control':
            continue
        a,b=matrix(two,features),matrix(previous,features)
        ya=np.array([e['zone_id'] for e in two]);yb=np.array([e['zone_id'] for e in previous])
        transfers[name]={m:dict(block_to_earlier=score(yb,predict(a,ya,b,m),[1,2]),
                              earlier_to_block=score(ya,predict(b,yb,a,m),[1,2])) for m in ['centroid','1nn']}
    topology={}
    for k in ['rise_ms','width_ms','peak_counts','apparent_decay_tau_ms']:
        values=np.array([zone_stats[str(z)][k]['median'] for z in manifest['topology']])
        ranks=np.argsort(np.argsort(values))
        topology[k]=dict(path=manifest['topology'],medians=values.tolist(),
                         spearman=float(np.corrcoef(np.arange(7),ranks)[0,1]))
    # Downsample the existing digitized trace at every possible phase. No claim
    # that this emulates the ADC's hardware 10 SPS mode or a faster reference.
    thinning={}
    for stride in [2,4]:
        losses=[]
        for e in seven:
            t,raw=captures[e['file']]
            for phase in range(stride):
                ix=np.arange(phase,len(t),stride)
                for center,amplitude,sign in [(e['peak_s'],e['peak_counts'],1),(e['trough_s'],e['trough_counts'],-1)]:
                    keep=ix[(t[ix]>=center-.4)&(t[ix]<=center+.4)]
                    found=max(sign*(raw[keep]-e['baseline_counts']))
                    losses.append(100*(amplitude-found)/amplitude)
        thinning[stride]=describe(losses)
    # Fold-independent measurement sensitivity: shift local baseline by +/-2000
    # counts and phase-subsample are kept separate from model accuracy claims.
    ratio_sensitivity=max(abs((e['trough_counts']+delta)/(e['peak_counts']-delta)-e['trough_peak_ratio'])
                          for e in seven for delta in [-2000,2000])
    result=dict(version=1,python=platform.python_version(),numpy=np.__version__,quality=quality,
        detection_sensitivity=sensitivity,zone_statistics=zone_stats,separations=separations,
        families=FAMILIES,exploratory_checks=checks,coarse_mapping_of_seven_class_predictions=coarse,
        exploratory_four_class_checks=four_class,
        provisional_cross_recording=transfers,topology=topology,subsampling_peak_loss_percent=thinning,
        max_ratio_change_baseline_plusminus_2000=float(ratio_sensitivity),events=events,
        limitations=['One recording per zone: a proper leave-recording-out seven-class test is impossible.',
        'All feature families are exploratory; no family selected here has independent test accuracy.',
        'Earlier alternating labels are inferred from reported order, not synchronized ground truth.',
        'Peak alignment and feature crossings use the completed pulse; prefix vectors alone have bounded observation windows.',
        'The common detector uses the recording median offline; implement and validate a causal baseline separately.'])
    (out/'results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
    keys=sorted(set().union(*(e.keys() for e in events)))
    with (out/'events.csv').open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(events)
    (out/'families.json').write_text(json.dumps(FAMILIES,indent=2),encoding='utf-8')
    print('Events:',len(seven),'complete seven-zone;',len(previous),'provisional earlier tail/back')
    print('Family: centroid LOO / chronological / reverse; 1NN LOO / chronological')
    for name,r in checks.items():
        print(name,*[f"{r[m][k]['correct']}/{r[m][k]['n']}" for m,k in
                    [('centroid','loo'),('centroid','chronological'),('centroid','reverse_chronological'),('1nn','loo'),('1nn','chronological')]])
    print('Topology:',json.dumps(topology))
    print('Transfer:',json.dumps({k:{m:(v[m]['block_to_earlier']['accuracy'],v[m]['earlier_to_block']['accuracy']) for m in v} for k,v in transfers.items()}))
    return result,captures


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=DEFAULT)
    parser.add_argument('--out',type=Path,default=DEFAULT/'results')
    args=parser.parse_args()
    run(args.data,args.out)
