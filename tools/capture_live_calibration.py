"""Capture a known physical zone after the user's start, until stop/time limit.

The capture ends when the optional --stop file appears, after --seconds
(default 300), or on Ctrl+C; the .json summary is written in every case.
The caller creates --stop only in response to the user's explicit end message.
This utility sends no bytes to the MCU. Label is CSV metadata only.
"""
import argparse
import csv
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time
import serial
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'src/learning-tool'))
from capture import parse_line


def stop_requested(stop):
    return stop is not None and stop.exists()


def capture(label, output, stop=None, port='COM7', seconds=300):
    zones = {'HEAD':7, 'BACK':2, 'LEGS':3, 'TAIL':1}
    assert not stop_requested(stop), 'Stop marker already exists; use a new path.'
    output.parent.mkdir(parents=True, exist_ok=True)
    count = gaps = 0
    first = last = None
    with output.open('x', newline='', encoding='utf8') as f, output.with_suffix('.uart.log').open('x', encoding='utf8', buffering=1) as log, serial.Serial(port, 115200, timeout=.1) as dev:
        w = csv.writer(f)
        w.writerow(['host_utc','seq','mcu_ms','raw','label'])
        dev.reset_input_buffer()
        start = datetime.now(timezone.utc).isoformat()
        began = time.monotonic()
        interrupted = False
        print(label, 'capture started:', output, flush=True)
        try:
            while not stop_requested(stop) and time.monotonic()-began < seconds:
                line = dev.readline()
                if not line:
                    continue
                log.write(line.decode('ascii', errors='replace'))
                s = parse_line(line)
                if not s:
                    continue
                if first is None:
                    first = s
                if last and s.seq != ((last.seq+1) & 0xffffffff):
                    gaps += 1
                w.writerow([datetime.now(timezone.utc).isoformat(),s.seq,s.ms,s.raw,zones[label]])
                f.flush()
                last = s
                count += 1
        except KeyboardInterrupt:
            interrupted = True
        if interrupted:
            reason = 'keyboard_interrupt'
        else:
            reason = 'user_end' if stop_requested(stop) else 'safety_timeout'
        result = dict(label=label, zone=zones[label], file=str(output), samples=count,
            sequence_gaps=gaps, start_utc=start, end_utc=datetime.now(timezone.utc).isoformat(),
            stop_reason=reason, time_limit_s=seconds,
            sps=(count-1)*1000/(last.ms-first.ms) if count > 1 else None)
        output.with_suffix('.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        print(result, flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--label', choices=['HEAD','BACK','LEGS','TAIL'], required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--stop', type=Path, default=None)
    p.add_argument('--seconds', type=float, default=300)
    p.add_argument('--port', default='COM7')
    a = p.parse_args()
    capture(a.label, a.output, a.stop, a.port, a.seconds)
