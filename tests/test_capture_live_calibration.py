"""capture_live_calibration ends on Ctrl+C / time limit and still writes its summary."""
import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

try:
    import serial  # noqa: F401
except ImportError as exc:
    raise unittest.SkipTest('pyserial not installed') from exc

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import capture_live_calibration as clc  # noqa: E402


class FakeSerial:
    def __init__(self, lines, *args, **kwargs):
        self.lines = list(lines)
        self.written = []

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def reset_input_buffer(self):
        pass

    def write(self, data):
        self.written.append(data)

    def readline(self):
        if not self.lines:
            return b''
        item = self.lines.pop(0)
        if isinstance(item, BaseException):
            raise item
        return item


class CaptureLiveCalibrationTests(unittest.TestCase):
    def run_capture(self, lines, **kwargs):
        fake = FakeSerial(lines)
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        out = Path(tmp.name) / 'head-1.csv'
        with patch.object(clc.serial, 'Serial', lambda *a, **k: fake), \
                contextlib.redirect_stdout(io.StringIO()):
            clc.capture('HEAD', out, **kwargs)
        self.assertEqual(fake.written, [])
        return out, json.loads(out.with_suffix('.json').read_text(encoding='utf8'))

    def test_keyboard_interrupt_writes_summary(self):
        lines = [b'PNEU1,1,100,5\r\n', b'# PNEE1,1,START,100\r\n', b'PNEU1,2,125,6\r\n', KeyboardInterrupt()]
        out, meta = self.run_capture(lines, seconds=60)
        self.assertEqual(meta['stop_reason'], 'keyboard_interrupt')
        self.assertEqual(meta['samples'], 2)
        self.assertIn('# PNEE1,1,START,100', out.with_suffix('.uart.log').read_text(encoding='utf8'))

    def test_time_limit_without_stop_file(self):
        out, meta = self.run_capture([b'PNEU1,1,100,5\r\n'], seconds=0)
        self.assertEqual(meta['stop_reason'], 'safety_timeout')
        self.assertEqual(meta['samples'], 0)


if __name__ == '__main__':
    unittest.main()
