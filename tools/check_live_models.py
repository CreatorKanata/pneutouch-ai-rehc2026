"""Check the compiled two-stage firmware on the real chip, explicitly in PAI1.

The model trains its own FLASH tables. Send raw float32 features + sensor peak,
never teacher labels. Normal acquisition resumes and retrains on exit.
This is training-set reconstruction/integration verification, not test accuracy.
"""
import hashlib
import json
from pathlib import Path
import time
import numpy as np
from validate_solist_ai import Device, decode

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/analysis/2026-09-20-live-calibration'


def run(port='COM7'):
    source = BASE / 'pressure-demo-training.json'
    data = json.loads(source.read_text(encoding='utf8'))
    baseline = json.loads((BASE / 'deployment-fit-check.json').read_text(encoding='utf8'))
    report = dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        meaning='Compiled firmware, two resident AI instances, training-set reconstruction only.',
        port=port, events=[])
    dest = BASE / 'specialist-firmware-check.json'
    with dest.with_suffix('.uart.log').open('w', encoding='utf8', buffering=1) as log:
        device = Device(port, log)
        try:
            device.command('MODE', 1)
            device.command('LIVEINIT', 1)
            for i, event in enumerate(data['events']):
                values = np.asarray(event['features12'] + [event['peak']], dtype='<f4')
                payload = ''.join(f'{int(v):08x}' for v in values.view('<u4'))
                response = device.command('LIVEPREDICT', i+2, payload)
                label, primary, refined, elapsed = map(int, response[3:7])
                assert primary == baseline['predictions'][i], (i, primary, baseline['predictions'][i])
                if primary in (1, 2):
                    assert label == primary and refined == 0
                else:
                    assert label in (3, 4) and refined == 1
                report['events'].append(dict(index=i, target=event['label'], label=label,
                    primary=primary, specialist_used=bool(refined), elapsed_us=elapsed,
                    scores=decode(response[7:11]).tolist()))
            y = np.array([e['target'] for e in report['events']])
            p = np.array([e['label'] for e in report['events']])
            report.update(correct=int((y == p).sum()), total=len(y),
                confusion_matrix=[[int(((y == a) & (p == b)).sum()) for b in (1, 2, 3, 4)] for a in (1, 2, 3, 4)],
                primary_predictions_match_previous_firmware=True, status='complete')
            print(json.dumps({k:report[k] for k in ('correct','total','confusion_matrix')},indent=2),flush=True)
        finally:
            try:
                device.command('MODE', 0)
                until = time.monotonic()+4
                samples, notifications = 0, []
                while time.monotonic() < until:
                    line = device.serial.readline().decode('ascii', errors='replace').strip()
                    if line.startswith('PNEU1,'): samples += 1
                    elif line: notifications.append(line)
                report['resumed_live'] = dict(samples=samples, notifications=notifications)
                print('Physical acquisition resumed:',samples,'samples',flush=True)
            finally:
                device.serial.close()
                dest.write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf8')


if __name__ == '__main__':
    run()
