"""Receive Phase 0 UART on macOS/Windows; print raw counts and save CSV safely."""
import argparse
import csv
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
import re
import math
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import config


@dataclass(frozen=True)
class Sample:
    seq: int
    ms: int
    raw: int


def parse_line(line):
    """Reject partial, wrong-version or out-of-range frames; never invent samples."""
    match = re.fullmatch(rb"PNEU1,([0-9]{1,10}),([0-9]{1,10}),(-?[0-9]{1,7})\r?\n", line)
    if match is None:
        return None
    seq, ms, raw = map(int, match.groups())
    if seq > 0xFFFFFFFF or ms > 0xFFFFFFFF or not -8388608 <= raw <= 8388607:
        return None
    return Sample(seq, ms, raw)


class LineBuffer:
    """Keep partial UART reads; discard an overlong frame through its newline."""
    def __init__(self):
        self.pending = bytearray()
        self.discarding = False
        self.dropped = 0

    def feed(self, chunk):
        for byte in chunk:
            if not self.discarding:
                self.pending.append(byte)
                if len(self.pending) > config.MAX_LINE_BYTES:
                    self.pending.clear()
                    self.discarding = True
            if byte == 10:
                if not self.discarding:
                    yield bytes(self.pending)
                else:
                    self.dropped += 1
                self.pending.clear()
                self.discarding = False


def capture(port, output, duration, serial_module, label=""):
    buffer = LineBuffer()
    count = 0
    previous = None
    started = last_sample = time.monotonic()
    # Exclusive-create prevents an accidental repeat from overwriting a dataset.
    with output.open("x", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(("host_utc", "seq", "mcu_ms", "raw", "label"))
        with serial_module.Serial(port, config.UART_BAUD, timeout=config.SERIAL_TIMEOUT_S,
                                  xonxoff=False, rtscts=False, dsrdtr=False) as connection:
            try:
                while duration is None or time.monotonic() - started < duration:
                    before = buffer.dropped
                    for line in buffer.feed(connection.read(max(1, min(connection.in_waiting, config.READ_CHUNK_BYTES)))):
                        if line.startswith(b"#"):
                            print(line.decode("ascii", errors="replace").strip(), file=sys.stderr)
                            continue
                        sample = parse_line(line)
                        if sample is None:
                            print(f"Ignored invalid frame: {line[:80]!r}", file=sys.stderr)
                            continue
                        if previous is not None and sample.seq != ((previous + 1) & 0xFFFFFFFF):
                            print(f"Sequence gap/reset: {previous} -> {sample.seq}", file=sys.stderr)
                        previous = sample.seq
                        writer.writerow((datetime.now(timezone.utc).isoformat(), sample.seq,
                                         sample.ms, sample.raw, label))
                        file.flush()
                        print(f"{sample.seq:8d}  {sample.ms:10d} ms  raw={sample.raw:9d}")
                        count += 1
                        last_sample = time.monotonic()
                    if buffer.dropped > before:
                        print("Discarded overlong serial frame", file=sys.stderr)
                    if time.monotonic() - last_sample >= config.NO_DATA_WARNING_S:
                        print("No samples: check power, CN9 channel B, USB routing and HX710B wiring.",
                              file=sys.stderr)
                        last_sample = time.monotonic()
            except KeyboardInterrupt:
                pass
    print(f"Saved {count} samples to {output}", file=sys.stderr)
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="List serial ports")
    parser.add_argument("--port", help="e.g. /dev/cu.usbserial-... or COM5")
    parser.add_argument("--output", type=Path, required=False, help="New CSV filename (never overwritten)")
    parser.add_argument("--label", choices=[str(i) for i in range(8)], default="",
                        help="Physical zone 1-7, 0=no touch; omitted=unlabeled")
    parser.add_argument("--duration", type=float, help="Seconds; default until Ctrl+C")
    args = parser.parse_args()
    if args.duration is not None and (not math.isfinite(args.duration) or args.duration <= 0):
        parser.error("--duration must be positive")
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.exit(1, "Install dependencies: python -m pip install -r requirements.txt\n")
    if args.list:
        for port in sorted(list_ports.comports()):
            print(f"{port.device}\t{port.description}\t{port.hwid}")
        return
    if not args.port:
        parser.error("Choose a port with --list, then provide --port")
    if args.output is None:
        parser.error("Provide --output captures/pressure-001.csv")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    try:
        count = capture(args.port, args.output, args.duration, serial, args.label)
    except (OSError, serial.SerialException) as error:
        parser.exit(1, f"Capture failed: {error}\n")
    if not count:
        parser.exit(2, "No valid sensor samples received.\n")


if __name__ == "__main__":
    main()
