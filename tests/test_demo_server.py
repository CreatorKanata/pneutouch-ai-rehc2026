"""MP4 byte ranges and a bounded static document root, using a real local HTTP server."""
import http.client
from pathlib import Path
import threading
import unittest
from http.server import ThreadingHTTPServer
from tools.serve_demo import DemoHandler, ROOT


class QuietHandler(DemoHandler):
    def log_message(self, *args): pass


class DemoServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), QuietHandler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()

    def request(self, path, headers=None, method='GET'):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=5)
        conn.request(method, path, headers=headers or {})
        r = conn.getresponse(); result = r.status, dict(r.getheaders()), r.read()
        conn.close(); return result

    def test_app_config_modules_and_no_arbitrary_repository_files(self):
        self.assertEqual(self.request('/')[0], 302)
        for path in ['/demo-visualizer/', '/demo-visualizer/assets.json', '/shared/serial.mjs', '/config.json',
                     '/images/demo/normal.jpg', '/images/demo/pressed.jpg', '/images/demo/head.jpg']:
            self.assertEqual(self.request(path)[0], 200, path)
        for path in ['/docs/concept.md', '/.git/config', '/demo-visualizer/../../config.py',
                     '/demo-visualizer/%2e%2e/shared/serial.mjs', '/videos/demo/%5c..%5c..%5cconfig.py',
                     '/images/demo/%00.jpg']:
            self.assertEqual(self.request(path)[0], 404, path)

    def test_video_range_head_suffix_and_invalid_ranges(self):
        file = ROOT/'videos/demo/static.mp4'
        status, headers, data = self.request('/videos/demo/static.mp4', {'Range':'bytes=0-63'})
        self.assertEqual(status,206); self.assertEqual(len(data),64)
        self.assertEqual(headers['Content-Type'],'video/mp4')
        with file.open('rb') as f: self.assertEqual(data,f.read(64))
        self.assertEqual(headers['Content-Range'],f'bytes 0-63/{file.stat().st_size}')
        self.assertEqual(self.request('/videos/demo/static.mp4', {'Range':'bytes=-12'})[0],206)
        self.assertEqual(len(self.request('/videos/demo/static.mp4', {'Range':'bytes=-12'})[2]),12)
        self.assertEqual(self.request('/videos/demo/static.mp4',method='HEAD')[2],b'')
        for value in ['bytes=99999999999-', 'bytes=9-2', 'bytes=0-1,4-5', 'bytes=-0']:
            self.assertEqual(self.request('/videos/demo/static.mp4',{'Range':value})[0],416)


if __name__ == '__main__': unittest.main()
