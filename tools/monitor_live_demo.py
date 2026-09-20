"""Read-only COM7 monitor: physical sensor, chip classifications and LCD ACKs.

Never sends samples, commands, labels or training data to the firmware.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
import serial


def monitor(port, seconds, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    report = dict(start_utc=datetime.now(timezone.utc).isoformat(), port=port,
                  source='physical HX710B only; UART monitor sends no bytes',
                  samples=0, sequence_gaps=0, restarts=0, interval_counts={},
                  notifications=[], classifications=[], lcd=[], holds_ms=[])
    previous = None
    shown = None
    with serial.Serial(port, 115200, timeout=.2) as device, output.with_suffix('.uart.log').open('w', encoding='utf8', buffering=1) as log:
        start = time.monotonic()
        while time.monotonic()-start < seconds:
            raw = device.readline().decode('ascii', errors='replace').strip()
            if not raw:
                continue
            log.write(raw+'\n')
            if raw.startswith('PNEU1,'):
                try:
                    _, seq, ms, adc = raw.split(',')
                    seq, ms, adc = int(seq), int(ms), int(adc)
                except ValueError:
                    continue  # Initial attachment can split a UART line.
                report['samples'] += 1
                if previous:
                    if seq < previous[0]:
                        report['restarts'] += 1
                    else:
                        report['sequence_gaps'] += seq-previous[0]-1
                        dt = (ms-previous[1]) & 0xffffffff
                        report['interval_counts'][dt] = report['interval_counts'].get(dt, 0)+1
                previous = seq, ms
            elif raw.startswith('#'):
                print(raw, flush=True)
                report['notifications'].append(raw)
                if raw.startswith('# PNEC1,'):
                    f = raw.split(',')
                    report['classifications'].append(dict(event=int(f[1]), ms=int(f[2]),
                        label=f[3], predict_us=int(f[4]), scores_bf16=f[5:]))
                if raw.startswith('# PNEL1,'):
                    f = raw.split(',')
                    report['lcd'].append(raw)
                    if len(f) == 3 and f[2].isdigit():
                        if f[1] in ('TAIL','BACK','LEGS','HEAD'):
                            shown = int(f[2])
                        elif f[1] == 'UNKNOWN' and shown is not None:
                            report['holds_ms'].append((int(f[2])-shown) & 0xffffffff)
                            shown = None
                output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    report['elapsed_s'] = time.monotonic()-start
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({k:report[k] for k in ('samples','sequence_gaps','restarts','holds_ms')}, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--port', default='COM7')
    p.add_argument('--seconds', type=float, default=120)
    p.add_argument('--output', type=Path, default=Path('build/live-demo-monitor.json'))
    a = p.parse_args()
    monitor(a.port, a.seconds, a.output)
