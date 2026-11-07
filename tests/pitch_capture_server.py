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
