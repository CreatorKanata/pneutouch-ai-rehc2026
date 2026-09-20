"""Validate fragmented serial input and CSV output without sensor hardware."""
import contextlib
import csv
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import config
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src/learning-tool"))
from capture import LineBuffer, Sample, capture, parse_line
from tools.prepare_vendor import header


class CaptureTests(unittest.TestCase):
    def test_signed_limits_and_counters(self):
        self.assertEqual(parse_line(b"PNEU1,4294967295,4294967295,-8388608\r\n"),
                         Sample(4294967295, 4294967295, -8388608))
        self.assertEqual(parse_line(b"PNEU1,0,0,8388607\n"), Sample(0, 0, 8388607))

    def test_invalid_frames_are_not_samples(self):
        for line in (b"PNEU1,1,2,3", b"PNEU2,1,2,3\n", b"PNEU1,1,2,8388608\n",
                     b"PNEU1,4294967296,0,0\n", b"PNEU1,1,2,3,4\n",
                     b"# ERROR,HX710B_TIMEOUT\n", b"PNEU1,1,2,nan\n"):
            with self.subTest(line=line):
                self.assertIsNone(parse_line(line))

    def test_split_reads_and_overlong_recovery(self):
        buffer = LineBuffer()
        self.assertEqual(list(buffer.feed(b"PNEU1,1,")), [])
        self.assertEqual(list(buffer.feed(b"20,-3\r\n")), [b"PNEU1,1,20,-3\r\n"])
        self.assertEqual(list(buffer.feed(b"x" * (config.MAX_LINE_BYTES + 1))), [])
        self.assertEqual(list(buffer.feed(b"discard\nPNEU1,2,45,6\n")),
                         [b"PNEU1,2,45,6\n"])

    def test_capture_csv_and_sequence_warning(self):
        class FakeSerial:
            in_waiting = 64
            def __init__(self, *args, **kwargs):
                self.chunks = iter((b"# started\nPNEU1,0,600,-3\nPNEU1,",
                                    b"2,650,123\n"))
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def read(self, size):
                try: return next(self.chunks)
                except StopIteration: raise KeyboardInterrupt

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "pressure.csv"
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                self.assertEqual(capture("fake", output, None,
                                         SimpleNamespace(Serial=FakeSerial)), 2)
            with output.open() as file:
                rows = list(csv.DictReader(file))
            self.assertEqual([row["raw"] for row in rows], ["-3", "123"])
            self.assertIn("Sequence gap/reset", stderr.getvalue())
            with self.assertRaises(FileExistsError):
                capture("fake", output, 1, SimpleNamespace(Serial=FakeSerial))

    def test_rate_configuration(self):
        with patch.object(config, "SAMPLE_RATE_HZ", 10):
            self.assertIn("PNEU_HX_PULSES 25U", header())
        with patch.object(config, "SAMPLE_RATE_HZ", 40):
            self.assertIn("PNEU_HX_PULSES 27U", header())
        with patch.object(config, "SAMPLE_RATE_HZ", 200):
            with self.assertRaises(ValueError): header()


if __name__ == "__main__":
    unittest.main()
