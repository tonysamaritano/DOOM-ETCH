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
