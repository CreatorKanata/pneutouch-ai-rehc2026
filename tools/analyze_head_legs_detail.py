"""Offline, phase-by-phase HEAD/LEGS analysis of existing pressure captures.

No serial access and no firmware/model changes. Accepted event membership comes
from the previously frozen C-verified datasets. An instrumented host copy of the
same C extractor reports rejection causes without changing detector decisions.
Timing is relative to inferred pressure crossings, never a measured hand onset.
"""
import csv
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
from collections import Counter

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from analyze_pressure import load, crossing, integral, decay_fit

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / 'docs/analysis/2026-09-20-pressure'
LIVE = ROOT / 'docs/analysis/2026-09-20-live-calibration'
OUT = ROOT / 'docs/analysis/2026-09-20-head-legs-detail'
COLORS = {3: '#2879b8', 4: '#dd6b22'}
LABELS = {3: 'LEGS', 4: 'HEAD'}
PHASE_GRID = np.arange(-.4, .825, .025)


def diagnostic_executable(min_peak=100000):
    source = ROOT / 'src/pneutouch-solist/S_PneuTouch/pressure_features.c'
    text = source.read_text(encoding='utf8')
    instrumented = text.replace('#include <string.h>', '#include <string.h>\n#include <stdio.h>')
    before = 'if (a < 100000.0f || b < 30000.0f) return false;'
    after = (f'if (a < {min_peak}.0f || b < 30000.0f) {{ '
             'fprintf(stderr, "REJECT,%u,AMPLITUDE,%.8g,%.8g\\n", '
             '(unsigned)s->event.id, (double)a, (double)b); return false; }')
    assert instrumented.count(before) == 1
    instrumented = instrumented.replace(before, after)
    build = ROOT / 'build'; build.mkdir(exist_ok=True)
    cfile = build / f'head_legs_diagnostic_features_{min_peak}.c'
    cfile.write_text(instrumented, encoding='utf8')
    exe = build / f'head_legs_diagnostic_{min_peak}.exe'
    cc = shlex.split(os.environ.get('CC', 'cc'))
    subprocess.run(cc + ['-std=c11', '-O2', '-I', str(source.parent),
                        str(ROOT/'tests/replay_pressure_features.c'), str(cfile), '-o', str(exe)], check=True)
    return exe, hashlib.sha256(source.read_bytes()).hexdigest()


def quantiles(values):
    a = np.array([x for x in values if x is not None and np.isfinite(x)])
    return dict(n=len(a), q=np.quantile(a, [0, .25, .5, .75, 1]).tolist()) if len(a) else dict(n=0, q=[])


def auc(head, legs):
    a = np.array([x for x in head if x is not None and np.isfinite(x)])
    b = np.array([x for x in legs if x is not None and np.isfinite(x)])
    return float(np.mean(a[:, None] > b) + .5*np.mean(a[:, None] == b)) if len(a) and len(b) else None


def details(crow, t, raw, first_ms):
    trigger = ((int(crow[2])-int(first_ms)) % 2**32)/1000
    end = ((int(crow[3])-int(first_ms)) % 2**32)/1000
    base, a, b = crow[16:19]
    z = raw-base
    start = int(np.searchsorted(t, trigger-.028))
    stop = int(np.searchsorted(t, end, side='right'))
    peak = start + int(np.argmax(z[start:stop]))
    trough = peak + int(np.argmin(z[peak:stop]))
    lo = int(np.searchsorted(t, t[peak]-1.2))
    c = {}
    for level in (.1, .5, .9):
        c[f'pa{level}'] = crossing(t, z, level*a, lo, peak, True, True)
        c[f'pd{level}'] = crossing(t, z, level*a, peak, trough, False)
        c[f'na{level}'] = crossing(t, -z, level*b, peak, trough, True, True)
        c[f'nd{level}'] = crossing(t, -z, level*b, trough, stop-1, False)
    assert all(v is not None for v in c.values()), (crow, c)
    assert abs((c['pa0.9']-c['pa0.1'])*1000-crow[4]) < .03
    assert abs((c['nd0.1']-c['nd0.9'])*1000-crow[11]) < .03
    pre = raw[(t >= trigger-.825) & (t <= trigger-.325)]
    tau, r2 = decay_fit(t, z, c['pd0.9'], c['pd0.1'])
    ntau, nr2 = decay_fit(t, -z, c['nd0.9'], c['nd0.1'])
    between = z[(t >= c['pd0.1']+.1) & (t <= c['na0.1']-.1)]
    post = z[(t >= t[trough]+.4) & (t <= t[trough]+.7)]
    late = z[(t >= t[trough]+.2) & (t <= t[trough]+.8)]
    positive_area = integral(t, z, c['pa0.1'], c['pd0.1'])
    negative_area = -integral(t, z, c['na0.1'], c['nd0.1'])
    result = dict(peak_counts=a, trough_counts=b, baseline_counts=base,
        baseline_sd=float(np.std(pre)), peak_to_trough_ms=(t[trough]-t[peak])*1000,
        pressure_onset_to_release_ms=(c['na0.1']-c['pa0.1'])*1000,
        positive_area_counts_s=positive_area, negative_area_counts_s=negative_area,
        positive_tau_ms=tau, positive_tau_r2=r2, negative_tau_ms=ntau, negative_tau_r2=nr2,
        between_pulses_fraction=float(np.median(between)/a) if len(between) >= 3 else None,
        post_400_700_fraction=float(np.median(post)/b) if len(post) >= 5 else None,
        rebound_200_800_fraction=float(np.max(late)/b) if len(late) >= 10 else None,
        rise_sample_intervals=crow[4]/(1000*np.median(np.diff(t))),
        recovery_sample_intervals=crow[11]/(1000*np.median(np.diff(t))),
        trigger_s=trigger, end_s=end, peak_s=float(t[peak]), trough_s=float(t[trough]))
    waves = dict(press=np.interp(t[peak]+PHASE_GRID, t, z, left=np.nan, right=np.nan),
                 release=np.interp(t[trough]+PHASE_GRID, t, z, left=np.nan, right=np.nan))
    return result, waves


def phase_plot(events, waves):
    fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    for label in (3, 4):
        chosen = [e for e in events if e['cohort'] == 'live' and e['label'] == label]
        for col, phase in enumerate(('press', 'release')):
            raw = np.array([waves[e['key']][phase] for e in chosen])
            norm = np.array([e['peak_counts'] if phase == 'press' else e['trough_counts'] for e in chosen])
            for row, values in enumerate((raw/1000, raw/norm[:, None])):
                q1, med, q3 = np.nanquantile(values, [.25, .5, .75], axis=0)
                ax = axes[row, col]
                ax.plot(PHASE_GRID, med, color=COLORS[label], label=f'{LABELS[label]} n={len(chosen)}')
                ax.fill_between(PHASE_GRID, q1, q3, color=COLORS[label], alpha=.18)
    for row in range(2):
        for col in range(2):
            ax = axes[row, col]; ax.axhline(0, color='grey', lw=.7); ax.axvline(0, color='grey', lw=.7)
            ax.grid(alpha=.15); ax.legend(frameon=False, fontsize=9)
            ax.set(xlabel='Time from positive peak (s)' if col == 0 else 'Time from negative peak (s)',
                   ylabel='Baseline-subtracted pressure (1,000 counts)' if row == 0 else 'Pressure / peak magnitude')
    axes[0,0].set_title('Press-side pulse'); axes[0,1].set_title('Release-side pulse')
    fig.suptitle('Current captures: median and middle 50% of waveforms (not confidence bands)')
    fig.savefig(OUT/'phase-overlays.png', dpi=160); plt.close(fig)


def normalized_distances(events, waves):
    """Descriptive distances, not independent trials or a trained classifier."""
    result = {}
    for cohort in ('original', 'live'):
        result[cohort] = {}
        for phase, low, high in [('press', -.25, .35), ('release', 0, .35)]:
            grid = (PHASE_GRID >= low-1e-8) & (PHASE_GRID <= high+1e-8)
            samples = {}
            for label in (3, 4):
                es = [e for e in events if e['cohort'] == cohort and e['label'] == label]
                denom = 'peak_counts' if phase == 'press' else 'trough_counts'
                samples[label] = np.array([waves[e['key']][phase][grid]/e[denom] for e in es])
                assert np.isfinite(samples[label]).all()
            distances = {}
            for name, la, lb in [('within_head', 4, 4), ('within_legs', 3, 3), ('between', 4, 3)]:
                a, b = samples[la], samples[lb]
                d = np.sqrt(np.mean((a[:, None, :]-b[None, :, :])**2, axis=2))
                values = d[np.triu_indices(len(a), 1)] if la == lb else d.ravel()
                distances[name] = quantiles(values)
            result[cohort][phase] = dict(window_s=[low, high], **distances)
    return result


def threshold_sensitivity(records, baseline_replays):
    result = []
    for limit in (100000, 90000, 75000, 50000):
        exe = diagnostic_executable(limit)[0] if limit != 100000 else None
        variant = dict(min_positive_peak_counts=limit, recordings=[])
        for record in records:
            cohort, filename = record['cohort'], record['file']
            original = baseline_replays[(cohort, filename)]
            if exe:
                path = (OLD/'raw' if cohort == 'original' else LIVE)/filename
                run = subprocess.run([str(exe), str(path)], capture_output=True, text=True, check=True)
                replay = [[float(v) for v in line.split(',')] for line in run.stdout.splitlines()]
            else:
                replay = original
            # Completion gate must not alter event starts, ends, IDs or existing features.
            assert len(replay) == len(original)
            changed = []
            for old, new in zip(original, replay):
                assert old[1:4] == new[1:4]
                if int(old[0]) == 2: assert old == new
                if old[0] != new[0]:
                    assert old[0] == 3 and new[0] == 2
                    changed.append(dict(event=int(new[1]), peak_counts=new[17], trough_counts=new[18],
                                        features12=new[4:16]))
            variant['recordings'].append(dict(cohort=cohort, file=filename,
                replay_status_counts=dict(Counter(int(row[0]) for row in replay)), newly_valid=changed))
        result.append(variant)
    return result


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    exe, source_hash = diagnostic_executable()
    old_data = json.loads((OLD/'results/solist-inputs.json').read_text(encoding='utf8'))
    live_data = json.loads((LIVE/'pressure-demo-training.json').read_text(encoding='utf8'))
    expected = []
    for cohort, source in [('original', old_data), ('live', live_data)]:
        for e in source['events']:
            if e['label'] in (3, 4): expected.append(dict(e, cohort=cohort))
    records, events, rejects, waves, baseline_replays = [], [], [], {}, {}
    names = old_data['feature_order']
    for cohort, filename in sorted(set((e['cohort'], e['file']) for e in expected)):
        path = (OLD/'raw' if cohort == 'original' else LIVE)/filename
        rows, seq, ms, t, raw = load(path)
        replay = subprocess.run([str(exe), str(path)], capture_output=True, text=True, check=True)
        baseline_replays[(cohort, filename)] = [[float(v) for v in line.split(',')] for line in replay.stdout.splitlines()]
        causes = {}
        for line in replay.stderr.splitlines():
            f = line.split(',')
            if f[0] == 'REJECT': causes[int(f[1])] = dict(reason=f[2], peak_counts=float(f[3]), trough_counts=float(f[4]))
        wanted = {e['event']: e for e in expected if e['cohort'] == cohort and e['file'] == filename}
        statuses = Counter()
        matched = 0
        for line in replay.stdout.splitlines():
            row = [float(v) for v in line.split(',')]
            result, eid = int(row[0]), int(row[1]); statuses[result] += 1
            if result in (3,4):
                rejects.append(dict(cohort=cohort, file=filename, event=eid, status=result,
                                    **causes.get(eid, dict(reason='OTHER_OR_TIMEOUT'))))
            if result != 2 or eid not in wanted: continue
            e = wanted[eid]
            assert np.allclose(row[4:16], e['features12'], rtol=1e-5, atol=.001), (cohort,filename,eid)
            values, wave = details(row, t, raw, ms[0])
            key = f'{cohort}:{filename}:{eid}'
            record = dict(key=key, cohort=cohort, file=filename, event=eid,
                          label=e['label'], zone=e.get('zone'), **dict(zip(names,row[4:16])), **values)
            events.append(record); waves[key] = wave; matched += 1
        assert matched == len(wanted), (filename, matched, len(wanted))
        records.append(dict(cohort=cohort,file=filename,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            samples=len(t), sequence_gaps=int(np.sum(np.maximum(np.diff(seq)-1,0))),
            median_interval_ms=float(np.median(np.diff(t))*1000), valid_adopted=matched,
            replay_status_counts=dict(statuses)))
    metrics = names + ['peak_counts','trough_counts','positive_area_counts_s','negative_area_counts_s',
        'peak_to_trough_ms','pressure_onset_to_release_ms','positive_tau_ms','negative_tau_ms',
        'between_pulses_fraction','post_400_700_fraction','rebound_200_800_fraction']
    comparisons = {}
    for cohort in ('original','live'):
        h = [e for e in events if e['cohort'] == cohort and e['label'] == 4]
        l = [e for e in events if e['cohort'] == cohort and e['label'] == 3]
        comparisons[cohort] = {f:dict(head=quantiles([e[f] for e in h]),legs=quantiles([e[f] for e in l]),
                                   auc_head_greater=auc([e[f] for e in h],[e[f] for e in l])) for f in metrics}
    zones = {}
    for zone in (3,4,5,6,7):
        es = [e for e in events if e['cohort']=='original' and e['zone']==zone]
        zones[str(zone)] = {f:quantiles([e[f] for e in es]) for f in metrics}
    # Greedy disjoint matching by closest positive amplitude, with a 25% caliper.
    h=[e for e in events if e['cohort']=='live' and e['label']==4]
    l=[e for e in events if e['cohort']=='live' and e['label']==3]
    candidates=sorted((abs(np.log(a['peak_counts']/b['peak_counts'])),i,j)
                      for i,a in enumerate(h) for j,b in enumerate(l))
    used_h,used_l,pairs=set(),set(),[]
    for distance,i,j in candidates:
        if distance > np.log(1.25) or i in used_h or j in used_l: continue
        used_h.add(i); used_l.add(j)
        pairs.append(dict(head=h[i]['key'],legs=l[j]['key'],amplitude_ratio=h[i]['peak_counts']/l[j]['peak_counts'],
                          head_minus_legs={f:(h[i][f]-l[j][f] if h[i][f] is not None and l[j][f] is not None else None) for f in metrics}))
    source_groups = {}
    for record in records:
        es=[e for e in events if e['file']==record['file']]
        source_groups[record['file']]={f:quantiles([e[f] for e in es]) for f in ['peak_counts','trough_counts','decay_ms','release_rise_ms','recovery_ms','baseline_counts']}
    sensitivity = threshold_sensitivity(records, baseline_replays)
    report=dict(source_extractor_sha256=source_hash, records=records, events=events, rejects=rejects,
        comparisons=comparisons,original_zones=zones,amplitude_matched_pairs=pairs,source_groups=source_groups,
        normalized_waveform_distances=normalized_distances(events, waves), threshold_sensitivity=sensitivity,
        limitations=['Same-session single-capture confounding; no independent test.',
            'Human contact/release timestamps and force were not measured.',
            'Additional post-release features use up to 0.8 seconds after trough.',
            'AUC describes pairwise ordering; it is not classifier validation accuracy.',
            'Current LEGS capture has no per-event foot identity.',
            'Videos have no documented time synchronization to UART and are not used as labels.'])
    (OUT/'analysis.json').write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf8')
    with (OUT/'events.csv').open('w',encoding='utf8',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(events[0]));writer.writeheader();writer.writerows(events)
    with (OUT/'aligned-waveforms.csv').open('w', encoding='utf8', newline='') as f:
        writer=csv.writer(f); writer.writerow(['event_key','phase','time_from_peak_s','baseline_subtracted_counts','normalized_pressure'])
        for e in events:
            for phase in ('press','release'):
                denom = e['peak_counts'] if phase == 'press' else e['trough_counts']
                for time, value in zip(PHASE_GRID, waves[e['key']][phase]):
                    writer.writerow([e['key'],phase,f'{time:.3f}',float(value),float(value/denom)])
    phase_plot(events,waves)
    fig,ax=plt.subplots(figsize=(8,8),constrained_layout=True)
    shown=['peak_counts','trough_counts','rise_ms','decay_ms','width_ms','asymmetry','release_rise_ms','recovery_ms',
           'release_width_ms','trough_peak_ratio','area_ratio','peak_to_trough_ms','positive_tau_ms','negative_tau_ms',
           'post_400_700_fraction','rebound_200_800_fraction']
    arr=np.array([[comparisons[c][f]['auc_head_greater'] for c in ('original','live')] for f in shown],dtype=float)
    im=ax.imshow(arr,vmin=0,vmax=1,cmap='coolwarm',aspect='auto')
    ax.set_yticks(range(len(shown)),shown);ax.set_xticks([0,1],['Original 7-zone session','Current labelled session'])
    for i in range(len(shown)):
        for j in range(2): ax.text(j,i,f'{arr[i,j]:.2f}',ha='center',va='center')
    fig.colorbar(im,ax=ax,label='P(HEAD value > LEGS value); 0.5 = no ordering')
    ax.set_title('Feature direction and separation can change between captures')
    fig.savefig(OUT/'feature-ordering.png',dpi=160);plt.close(fig)
    fig,axes=plt.subplots(1,3,figsize=(13,4.5),constrained_layout=True)
    groups=[('original',z) for z in (3,4,5,6,7)]+[('live',3),('live',4)]
    group_names=['R hind','L hind','R front','L front','Head','Live legs','Live head']
    for ax,f,title in zip(axes,['peak_counts','decay_ms','recovery_ms'],['Positive peak (1,000 counts)','Press decay (ms)','Release recovery (ms)']):
        values=[]
        for cohort,zone in groups:
            a=[e[f] for e in events if e['cohort']==cohort and (e['zone']==zone if cohort=='original' else e['label']==zone)]
            values.append(np.array(a)/(1000 if f=='peak_counts' else 1))
        ax.boxplot(values,tick_labels=group_names,showfliers=True)
        ax.tick_params(axis='x',rotation=45);ax.set_title(title);ax.grid(axis='y',alpha=.2)
    fig.suptitle('Individual feet and capture-to-capture differences')
    fig.savefig(OUT/'feet-and-capture-differences.png',dpi=160);plt.close(fig)
    if pairs:
        bykey={e['key']:e for e in events}
        fig,axes=plt.subplots(len(pairs),2,figsize=(10,3*len(pairs)),squeeze=False,constrained_layout=True)
        for row,pair in enumerate(pairs):
            for label,key in [(4,pair['head']),(3,pair['legs'])]:
                e=bykey[key]
                for col,phase in enumerate(('press','release')):
                    denom=e['peak_counts'] if phase=='press' else e['trough_counts']
                    axes[row,col].plot(PHASE_GRID,waves[key][phase]/denom,color=COLORS[label],label=f'{LABELS[label]} {e["peak_counts"]/1000:.0f}k')
                    axes[row,col].grid(alpha=.2);axes[row,col].legend(fontsize=8)
                    axes[row,col].set(xlabel=f'Time from {phase} peak (s)',ylabel='Normalized pressure')
            axes[row,0].set_title(f'Pair {row+1}, amplitude ratio {pair["amplitude_ratio"]:.2f}')
        fig.suptitle('Disjoint amplitude-matched pairs (positive peaks within 25%)')
        fig.savefig(OUT/'amplitude-matched-pairs.png',dpi=160);plt.close(fig)
    print('Analysed',len(events),'C-matched accepted events from',len(records),'recordings')
    print('Counts:',dict(Counter((e['cohort'],e['label']) for e in events)))
    print('Rejects:',rejects)
    print('Amplitude-matched pairs:',len(pairs))
    print('Threshold sensitivity:', [(v['min_positive_peak_counts'], sum(len(r['newly_valid']) for r in v['recordings'])) for v in sensitivity])
    print('Normalized shape distances:', report['normalized_waveform_distances'])
    for f in shown:
        print(f,[(c,round(comparisons[c][f]['auc_head_greater'],3),
                     round(comparisons[c][f]['head']['q'][2],3),round(comparisons[c][f]['legs']['q'][2],3)) for c in ('original','live')])


if __name__=='__main__':
    run()
