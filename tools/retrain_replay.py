"""Run the host build of the firmware feature extractor on a capture CSV.

The executable is tests/replay_pressure_features.c compiled with the same
pressure_features.c as the firmware. It prints one line per event:

    result,id,trigger_ms,ms[,f1..f12,baseline,peak,trough]

result 1=START 2=COMPLETE 3=INVALID 4=TIMEOUT; ``ms`` is the sample time at
which the result was produced. Feature values are only present for COMPLETE.
"""
import hashlib
import subprocess
from pathlib import Path

RESULTS = {1: 'start', 2: 'complete', 3: 'invalid', 4: 'timeout'}
FEATURE_COUNT = 12
COMPLETE_FIELDS = 4 + FEATURE_COUNT + 3


def parse_replay_line(line):
    """One replay line as a dict, or None if it is not a valid event line."""
    f = line.strip().split(',')
    try:
        result, eid, trigger, ms = (int(v) for v in f[:4])
        values = [float(v) for v in f[4:]]
    except ValueError:
        return None
    kind = RESULTS.get(result)
    if kind is None or (kind == 'complete') != (len(f) == COMPLETE_FIELDS) or (kind != 'complete' and len(f) != 4):
        return None
    row = dict(kind=kind, event=eid, trigger_ms=trigger, ms=ms)
    if kind == 'complete':
        row = dict(row, features12=values[:FEATURE_COUNT], baseline=values[FEATURE_COUNT],
                   peak=values[FEATURE_COUNT + 1], trough=values[FEATURE_COUNT + 2])
    return row


def parse_replay_output(text):
    return [r for r in map(parse_replay_line, text.splitlines()) if r is not None]


def run_replay(exe, csv_path):
    """Replay rows for one CSV; raises RuntimeError with the file name on failure."""
    try:
        done = subprocess.run([str(exe), str(csv_path)], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError(f'replay {exe} failed on {Path(csv_path).name}: {exc}') from exc
    return parse_replay_output(done.stdout)


def describe(exe):
    """Source description stored in the JSON note and summary.md."""
    path = Path(exe).resolve()
    return dict(kind='replay', exe=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
