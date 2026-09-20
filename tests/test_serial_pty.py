"""Optional POSIX integration: real pySerial receives fragmented synthetic UART bytes."""
import contextlib
import csv
import importlib.util
import io
import os
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src/learning-tool"))
from capture import capture


@unittest.skipUnless(os.name == "posix" and importlib.util.find_spec("serial"),
                     "Requires POSIX pseudo-terminal and optional pySerial")
class SerialIntegration(unittest.TestCase):
    def test_real_serial_fragmented_input(self):
        import serial
        master, slave = os.openpty()
        connected = threading.Event()

        class Port(serial.Serial):
            def __enter__(self):
                result = super().__enter__()
                connected.set()
                return result

        def send():
            if connected.wait(2):
                for block in (b"# synthetic test only\r\nPNEU1,0,", b"600,-123\r\n",
                              b"PNEU1,1,625,456\r\n"):
                    os.write(master, block)

        worker = threading.Thread(target=send)
        worker.start()
        try:
            with tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "synthetic.csv"
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    count = capture(os.ttyname(slave), output, 0.3, SimpleNamespace(Serial=Port), "7")
                with output.open() as file: rows = list(csv.DictReader(file))
                self.assertEqual(count, 2)
                self.assertEqual([r["raw"] for r in rows], ["-123", "456"])
                self.assertEqual([r["label"] for r in rows], ["7", "7"])
        finally:
            worker.join(timeout=3)
            os.close(master); os.close(slave)


if __name__ == "__main__": unittest.main()
