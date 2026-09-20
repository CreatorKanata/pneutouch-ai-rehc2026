"""Serve only the PC console on localhost; runtime settings come from config.py."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            self.send_response(302)
            self.send_header("Location", "/learning-tool/")
            self.end_headers()
        elif self.path == "/config.json":
            data = json.dumps({
                "baudRate": config.UART_BAUD, "maxLine": config.MAX_LINE_BYTES,
                "displaySamples": config.DISPLAY_SAMPLES,
                "recordingSamples": config.RECORDING_SAMPLES,
                "noDataMs": config.NO_DATA_WARNING_S * 1000,
                "nominalSps": config.SAMPLE_RATE_HZ,
            }).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)
        elif self.path.split('?')[0].startswith(("/learning-tool/", "/shared/")):
            super().do_GET()
        else:
            self.send_error(404)


if __name__ == "__main__":
    handler = partial(Handler, directory=str(ROOT / "src"))
    with ThreadingHTTPServer(("127.0.0.1", config.CONSOLE_PORT), handler) as server:
        print(f"Learning: http://localhost:{config.CONSOLE_PORT}/learning-tool/", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
