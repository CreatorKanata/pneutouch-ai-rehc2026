"""Causal sample-by-sample replay: no supplied touch times or zone labels.

Detector and 0.8 s vectors use only the observed prefix. Offline event labels are
used AFTER replay for matching/scoring, never to trigger or end a detected event.
This is pilot-data replay, not a live demo or an independent validation set.
"""
import json
from collections import deque

import numpy as np

from analyze_pressure import DEFAULT, load, describe, evaluate, extract, matrix, FAMILIES


class Detector:
    def __init__(self):
        self.quiet = deque(maxlen=80)
        self.warm = deque(maxlen=32)
        self.active = None
        self.rising = []
        self.ready = False
        self.ready_at = None
        self.quiet_count = 0
        self.returned_quiet = deque(maxlen=8)
        self.events = []

    def step(self, t, y):
        if not self.ready:
            self.warm.append(y)
            if len(self.warm)==32 and np.ptp(self.warm)<=20000:
                self.quiet.extend(self.warm)
                self.ready=True
                self.ready_at=t
            return
        if self.active is None:
            base=float(np.median(self.quiet))
            delta=y-base
            if delta>50000:
                if not self.rising:
                    self.pending_base=base
                self.rising.append((t,y))
                if len(self.rising)==2:
                    self.active=dict(trigger_s=t,threshold_first_s=self.rising[0][0],
                        baseline_counts=self.pending_base,negative_seen=False,
                        times=[t],values=[y],prefix=None)
                    self.rising=[]
                    self.quiet_count=0
                    self.returned_quiet.clear()
            else:
                self.rising=[]
                if abs(delta)<=25000:
                    self.quiet.append(y)
            return
        e=self.active
        e['times'].append(t)
        e['values'].append(y)
        delta=y-e['baseline_counts']
        if delta < -50000:
            e['negative_seen']=True
        if e['prefix'] is None and t-e['trigger_s']>=.8:
            # Interpolation endpoints are both already received at decision time.
            grid=e['trigger_s']+np.arange(0,.801,.025)
            v=np.interp(grid,e['times'],e['values'])-e['baseline_counts']
            e['prefix']=(v/max(float(np.max(np.abs(v))),1)).tolist()
            e['prefix_ready_s']=t
        if e['negative_seen'] and abs(delta)<=25000:
            self.quiet_count+=1
            self.returned_quiet.append(y)
        else:
            self.quiet_count=0
            self.returned_quiet.clear()
        if self.quiet_count>=8 or t-e['trigger_s']>=4:
            e['end_s']=t
            e['completion']='negative_then_quiet' if self.quiet_count>=8 else 'timeout'
            e.pop('times');e.pop('values')
            self.events.append(e)
            self.active=None
            self.rising=[]
            self.quiet_count=0
            self.quiet.clear()
            if e['completion']=='negative_then_quiet':
                # Keep the eight already observed quiet samples. Cold-starting
                # after every event creates unnecessary blind time for a demo.
                self.quiet.extend(self.returned_quiet)
            else:
                self.ready=False
                self.warm.clear()


def run():
    manifest=json.loads((DEFAULT/'manifest.json').read_text(encoding='utf8'))
    offline=json.loads((DEFAULT/'results/results.json').read_text(encoding='utf8'))
    results=[];matched=[]
    for meta in manifest['captures']:
        _,_,_,t,raw=load(DEFAULT/meta['file'])
        detector=Detector()
        for tt,yy in zip(t,raw):
            detector.step(float(tt),float(yy))
        for event in detector.events:
            # Extract at the detector's own completed-event deadline, before
            # labels/reference matching. All supplied samples precede that time.
            # This intentionally reuses a batch extractor on the past prefix;
            # it establishes causality, not bounded CPU/memory for MCU use.
            stop=int(np.searchsorted(t,event['end_s'],side='right'))
            unknown=dict(meta,kind='seven_zone',zone_id=0,label_basis='unknown during replay')
            candidates=extract(unknown,t[:stop],raw[:stop])
            current=[e for e in candidates if e['complete'] and
                     event['trigger_s']-.1<=e['peak_s']<=event['end_s']]
            event['completed_features']=current[0] if len(current)==1 else None
        reference=[e for e in offline['events'] if e['file']==meta['file'].split('/')[-1] and e['complete']]
        used=set()
        for event in detector.events:
            # Post-hoc reference matching is deliberately outside the detector.
            hits=[e for e in reference if e['event'] not in used and
                  event['trigger_s']-.1 <= e['peak_s'] <= event['end_s']]
            event['reference_event']=None
            if len(hits)==1:
                ref=hits[0]
                used.add(ref['event'])
                event['reference_event']=ref['event']
                event['zone_id']=ref['zone_id']
                event['trigger_minus_wave_10pct_ms']=(event['trigger_s']-ref['onset_s'])*1000
                event['end_minus_trigger_s']=event['end_s']-event['trigger_s']
                if meta['kind']=='seven_zone' and event['prefix'] is not None:
                    matched.append(event)
        results.append(dict(file=meta['file'],kind=meta['kind'],reference_complete=len(reference),
            detected=len(detector.events),matched=len(used),
            unmatched_detections=sum(e['reference_event'] is None for e in detector.events),
            unmatched_reference=[e['event'] for e in reference if e['event'] not in used],
            unfinished_at_eof=detector.active is not None,events=detector.events))
    x=np.array([e['prefix'] for e in matched])
    y=np.array([e['zone_id'] for e in matched])
    groups=np.array([1 if z==1 else 2 if z==2 else 4 if z==7 else 3 for z in y])
    scores={str(k):{m:evaluate(x,labels,matched,False,m,strata=y) for m in ['centroid','1nn']}
            for k,labels in [(7,y),(4,groups)]}
    full_events=[dict(e['completed_features'],zone_id=e['zone_id']) for e in matched if e['completed_features'] is not None]
    fy=np.array([e['zone_id'] for e in full_events])
    fg=np.array([1 if z==1 else 2 if z==2 else 4 if z==7 else 3 for z in fy])
    fx=matrix(full_events,FAMILIES['shape_ratios'])
    full_scores={str(k):{m:evaluate(fx,labels,full_events,True,m,strata=fy) for m in ['centroid','1nn']}
                 for k,labels in [(7,fy),(4,fg)]}
    report=dict(parameters=dict(warmup_points=32,warmup_max_span_counts=20000,
        baseline_points=80,trigger_counts=50000,trigger_consecutive=2,
        negative_counts=-50000,quiet_abs_counts=25000,quiet_consecutive=8,
        timeout_s=4,prefix_s=.8),records=results,
        completed_feature_count=len(full_events),exploratory_completed_classification=full_scores,
        seven_zone_timing={k:describe([e[k] for e in matched]) for k in
            ['trigger_minus_wave_10pct_ms','end_minus_trigger_s']},
        seven_zone_prefix_ready_delay_s=describe([e['prefix_ready_s']-e['trigger_s'] for e in matched]),
        exploratory_prefix_classification=scores,
        limitations=['Thresholds were chosen using this pilot dataset, not an independent validation set.',
        'Matches are against offline pressure-wave candidates, not physical contact ground truth.',
        'Each zone has only one recording; repeated events are dependent.',
        'No long idle, weak touch, held touch, overlapping touch or disconnect stress test.',
        'The four-second event timeout and stable startup require further validation for a demo.'])
    dest=DEFAULT/'results/causal-replay.json'
    dest.write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf8')
    print([(e['file'],e['detected'],e['matched'],e['unmatched_reference']) for e in results])
    print('Timing:',report['seven_zone_timing'])
    print('Prefix ready:',report['seven_zone_prefix_ready_delay_s'])
    for k,models in scores.items():
        print(k,{m:{s:(v[s]['correct'],v[s]['n']) for s in ['loo','chronological','reverse_chronological']} for m,v in models.items()})
    print('Completed features:',len(full_events))
    for k,models in full_scores.items():
        print(k,{m:{s:(v[s]['correct'],v[s]['n']) for s in ['loo','chronological','reverse_chronological']} for m,v in models.items()})
    return report


if __name__=='__main__':
    run()
