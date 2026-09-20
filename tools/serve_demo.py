"""Local files only. USB UART is handled directly by the HTML app's Web Serial.

No model or serial receiver runs in Python. Range requests support seeking MP4s.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import mimetypes
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import config


class DemoHandler(BaseHTTPRequestHandler):
    def do_HEAD(self):
        self.serve_file(False)

    def do_GET(self):
        self.serve_file(True)

    def serve_file(self, body):
        path = unquote(urlsplit(self.path).path)
        if path == '/':
            self.send_response(302)
            self.send_header('Location', '/demo-visualizer/')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if path == '/config.json':
            data = json.dumps(dict(baudRate=config.UART_BAUD, noDataMs=config.NO_DATA_WARNING_S*1000)).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            if body: self.wfile.write(data)
            return
        directories = {'/demo-visualizer/': ROOT/'src/demo-visualizer',
                       '/shared/': ROOT/'src/shared', '/videos/demo/': ROOT/'videos/demo',
                       '/images/demo/': ROOT/'images/demo'}
        file = None
        for prefix, directory in directories.items():
            if not path.startswith(prefix): continue
            relative = path[len(prefix):]
            if '\\' in relative or '\x00' in relative or any(p in ('.', '..') for p in relative.split('/')):
                break
            candidate = (directory/(relative or 'index.html')).resolve()
            if candidate.is_relative_to(directory.resolve()) and candidate.is_file(): file = candidate
            break
        if file is None:
            self.send_error(404)
            return
        size = file.stat().st_size
        start, end, partial = 0, size-1, False
        requested = self.headers.get('Range')
        if requested:
            match = re.fullmatch(r'bytes=(\d*)-(\d*)', requested)
            try:
                if not match or not any(match.groups()): raise ValueError()
                left, right = match.groups()
                if left:
                    start = int(left)
                    end = min(size-1, int(right)) if right else size-1
                else:
                    count = int(right)
                    if not count: raise ValueError()
                    start = max(0, size-count)
                if start >= size or end < start: raise ValueError()
                partial = True
            except ValueError:
                self.send_response(416)
                self.send_header('Content-Range', f'bytes */{size}')
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
        media_type = {'.mjs': 'text/javascript'}.get(file.suffix) or mimetypes.guess_type(file.name)[0] or 'application/octet-stream'
        self.send_response(206 if partial else 200)
        self.send_header('Content-Type', media_type)
        self.send_header('Content-Length', str(end-start+1))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Cache-Control', 'no-cache')
        self.send_header('X-Content-Type-Options', 'nosniff')
        if partial: self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.end_headers()
        if body:
            try:
                with file.open('rb') as stream:
                    stream.seek(start)
                    remaining = end-start+1
                    while remaining:
                        chunk = stream.read(min(65536, remaining))
                        if not chunk: break
                        self.wfile.write(chunk)
                        remaining -= len(chunk)
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass  # Browsers cancel preloads and old videos on the next touch.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8001)
    args = parser.parse_args()
    with ThreadingHTTPServer(('127.0.0.1', args.port), DemoHandler) as server:
        print(f'PneutouchAI demo: http://localhost:{args.port}/demo-visualizer/', flush=True)
        print('USB UART: select COM7 in Chrome/Edge. Python does not open the port.', flush=True)
        try: server.serve_forever()
        except KeyboardInterrupt: pass


if __name__ == '__main__': main()
