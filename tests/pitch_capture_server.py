# SPDX-License-Identifier: GPL-2.0-or-later
"""Production protocol tests; header fixtures are NOT decoded JPEG images."""
import http.client
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / 'viewer/capture_server.py'
spec = importlib.util.spec_from_file_location('pitch_capture_server', SOURCE)
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)
JPEG = bytes.fromhex('ffd8 ffc0 000b 08 0438 0780 01 01 11 00 ffda 0008 01 01 00 003f00 ffd9')

class CaptureProtocol(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.root_patch = patch.object(capture, 'ROOT', self.root)
        self.root_patch.start()
        handler = capture.handler_for(self.root)
        handler.log_message = lambda *args: None
        self.server = capture.ThreadingHTTPServer(('127.0.0.1', 0), handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.frames = self.root / 'build/promo/pitch/frames'

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.root_patch.stop()
        self.temp.cleanup()

    def post(self, path, body=b'', headers=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.server.server_port)
        merged = {'Content-Length': str(len(body))}
        if body:
            merged['Content-Type'] = 'image/jpeg'
        merged.update(headers or {})
        connection.request('POST', path, body, merged)
        response = connection.getresponse()
        result = response.status, response.read()
        connection.close()
        return result

    def start(self):
        status, body = self.post('/__start?cut=pitch')
        self.assertEqual(status, 200)
        result = json.loads(body)
        self.assertEqual((result['ok'], result['cut']), (True, 'pitch'))
        return result['token']

    def frame(self, token, index, body=JPEG, headers=None):
        return self.post(f'/__frame?cut=pitch&token={token}&frame={index}', body, headers)

    def done(self, token):
        return self.post(f'/__done?cut=pitch&token={token}')

    def test_complete_sequential_capture(self):
        token = self.start()
        for index in range(540):
            status, body = self.frame(token, index)
            self.assertEqual(status, 200)
            self.assertEqual(json.loads(body), {'ok': True, 'cut': 'pitch', 'token': token, 'frame': index})
        self.assertEqual(sorted(p.name for p in self.frames.iterdir()), [f'{i:05d}.jpg' for i in range(540)])
        self.assertTrue(all(p.read_bytes() == JPEG for p in self.frames.iterdir()))
        status, body = self.done(token)
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)['frames'], 540)

    def test_retries_restart_and_unrelated_files(self):
        token = self.start()
        self.assertEqual(self.frame(token, 1)[0], 400)
        self.assertEqual(self.frame(token, 0)[0], 200)
        self.assertEqual(self.frame(token, 0)[0], 200)
        changed = JPEG[:-2] + b'changed' + JPEG[-2:]
        self.assertEqual(self.frame(token, 0, changed)[0], 400)
        self.assertEqual((self.frames / '00000.jpg').read_bytes(), JPEG)
        for name in ('notes.txt', '00540.jpg', 'other.jpg'):
            (self.frames / name).write_bytes(b'keep')
        new_token = self.start()
        self.assertNotEqual(new_token, token)
        self.assertFalse((self.frames / '00000.jpg').exists())
        self.assertEqual(sorted(p.name for p in self.frames.iterdir()), ['00540.jpg', 'notes.txt', 'other.jpg'])
        self.assertTrue(all(p.read_bytes() == b'keep' for p in self.frames.iterdir()))
        self.assertEqual(self.frame(token, 0)[0], 400)
        self.assertEqual(self.done(token)[0], 400)
        self.assertEqual(self.frame(new_token, 0)[0], 200)

    def test_rejected_routes_queries_and_origins(self):
        token = self.start()
        paths = ['/__start?cut=film', '/__start?cut=../pitch', '/__start?cut=pitch&cut=pitch',
                 '/__start?cut=pitch&extra=1', '/__frame?cut=pitch&frame=0',
                 '/__frame?cut=pitch&token=wrong&frame=0', '/../../__start?cut=pitch']
        paths += [f'/__frame?cut=pitch&token={token}&frame={value}' for value in ('-1', '540', '1.0', '', '../0')]
        for path in paths:
            with self.subTest(path=path):
                self.assertEqual(self.post(path)[0], 400)
        for headers in ({'Host': 'evil.example'}, {'Origin': 'http://evil.example'},
                        {'Sec-Fetch-Site': 'cross-site'}):
            self.assertEqual(self.frame(token, 0, headers=headers)[0], 400)
        origin = f'http://127.0.0.1:{self.server.server_port}'
        self.assertEqual(self.frame(token, 0, headers={'Origin': origin})[0], 200)

    def test_rejected_bodies_and_jpeg_headers(self):
        token = self.start()
        wrong_size = bytearray(JPEG)
        wrong_size[10] = 0x81
        malformed = [b'', b'not jpeg', JPEG[:-2], bytes(wrong_size),
                     JPEG[:4] + b'\xff\xff' + JPEG[6:], JPEG.replace(b'\xff\xda', b'\xff\xd9')]
        for body in malformed:
            with self.subTest(body=body):
                self.assertEqual(self.frame(token, 0, body)[0], 400)
        for headers in ({'Content-Type': 'image/png'}, {'Content-Length': '-1'},
                        {'Content-Length': str(capture.MAX_JPEG_BYTES + 1)},
                        {'Transfer-Encoding': 'chunked'}):
            self.assertEqual(self.frame(token, 0, headers=headers)[0], 400)
        self.assertEqual(self.post('/__start?cut=pitch', b'unexpected')[0], 400)
        self.assertEqual(list(self.frames.iterdir()), [])

    def test_incomplete_missing_and_empty_completion(self):
        token = self.start()
        self.assertEqual(self.done(token)[0], 400)
        for index in range(540):
            self.assertEqual(self.frame(token, index)[0], 200)
        last = self.frames / '00539.jpg'
        last.unlink()
        self.assertEqual(self.done(token)[0], 400)
        last.write_bytes(b'')
        self.assertEqual(self.done(token)[0], 400)
        last.write_bytes(JPEG)
        self.assertEqual(self.done(token)[0], 200)

    def test_atomic_replace_failure_keeps_old_file(self):
        token = self.start()
        path = self.frames / '00000.jpg'
        path.write_bytes(b'old output')
        with patch.object(capture.os, 'replace', side_effect=OSError('injected replace failure')):
            self.assertEqual(self.frame(token, 0)[0], 400)
        self.assertEqual(path.read_bytes(), b'old output')
        self.assertEqual(sorted(p.name for p in self.frames.iterdir()), ['00000.jpg'])
        self.assertEqual(self.frame(token, 0)[0], 200)

if __name__ == '__main__':
    unittest.main()
