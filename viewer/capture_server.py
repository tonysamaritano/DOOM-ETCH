#!/usr/bin/env python3
"""Read-only localhost viewer server; Python is outside the drawing path."""
import argparse
import json
import os
import secrets
import tempfile
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]

FRAME_COUNT = 540
MAX_JPEG_BYTES = 8 * 1024 * 1024

def jpeg_dimensions(data):
    """Validate bounded JPEG headers, not full entropy/image decoding."""
    if len(data) < 4 or data[:2] != b'\xff\xd8' or data[-2:] != b'\xff\xd9':
        raise ValueError('Expected complete JPEG envelope')
    offset, dimensions = 2, None
    while offset < len(data) - 2:
        if data[offset] != 255:
            raise ValueError('Invalid JPEG marker')
        while offset < len(data) and data[offset] == 255:
            offset += 1
        if offset >= len(data):
            break
        marker = data[offset]
        offset += 1
        if marker in (0, 216, 217) or 208 <= marker <= 215:
            raise ValueError('Unexpected JPEG header marker')
        if offset + 2 > len(data):
            break
        length = int.from_bytes(data[offset:offset+2], 'big')
        if length < 2 or offset + length > len(data):
            raise ValueError('Truncated JPEG header')
        if marker in (192, 193, 194):
            if length < 8 or dimensions is not None:
                raise ValueError('Invalid JPEG frame header')
            dimensions = (int.from_bytes(data[offset+5:offset+7], 'big'),
                          int.from_bytes(data[offset+3:offset+5], 'big'))
        if marker == 218:
            if dimensions != (1920, 1080):
                raise ValueError('JPEG must be 1920x1080')
            return dimensions
        offset += length
    raise ValueError('Missing JPEG scan header')

class PitchSession:
    def __init__(self):
        self.lock = threading.Lock()
        self.token = None
        self.next_frame = 0

    def directory(self):
        root = ROOT.resolve()
        directory = root / 'build/promo/pitch/frames'
        if not directory.resolve().is_relative_to(root):
            raise ValueError('Capture directory escapes repository')
        directory.mkdir(parents=True, exist_ok=True)
        return directory

    def start(self):
        self.token = None
        self.next_frame = 0
        directory = self.directory()
        for frame in range(FRAME_COUNT):
            path = directory / f'{frame:05d}.jpg'
            if path.is_symlink() or path.is_file():
                path.unlink()
            elif path.exists():
                raise ValueError('Owned frame path is not a file')
        self.token = secrets.token_urlsafe(24)
        return self.token

    def require(self, token):
        if not self.token or token != self.token:
            raise ValueError('Missing or stale capture token')

    def frame(self, token, index, data):
        self.require(token)
        if index > self.next_frame:
            raise ValueError('Frame must be sequential')
        jpeg_dimensions(data)
        path = self.directory() / f'{index:05d}.jpg'
        if index < self.next_frame:
            if path.is_symlink() or path.read_bytes() != data:
                raise ValueError('Retry differs from acknowledged frame')
            return
        fd, temporary = tempfile.mkstemp(prefix='.pitch-', dir=path.parent)
        try:
            with os.fdopen(fd, 'wb') as stream:
                stream.write(data)
                stream.flush()
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        self.next_frame += 1

    def done(self, token):
        self.require(token)
        if self.next_frame != FRAME_COUNT:
            raise ValueError('Capture is incomplete')
        directory = self.directory()
        for index in range(FRAME_COUNT):
            path = directory / f'{index:05d}.jpg'
            if path.is_symlink() or not path.is_file() or not path.stat().st_size:
                raise ValueError('Capture frame is missing or empty')

def handler_for(three):
    session = PitchSession()
    class Handler(SimpleHTTPRequestHandler):
        extensions_map = {**SimpleHTTPRequestHandler.extensions_map,
                          '.js': 'text/javascript', '.glb': 'model/gltf-binary'}
        def capture_query(self):
            host = self.headers.get('Host', '')
            allowed = {f'127.0.0.1:{self.server.server_port}',
                       f'localhost:{self.server.server_port}'}
            if self.client_address[0] not in ('127.0.0.1', '::1') or host not in allowed:
                raise ValueError('Capture requires loopback host')
            origin = self.headers.get('Origin')
            if origin is not None and origin != f'http://{host}':
                raise ValueError('Capture requires same origin')
            if self.headers.get('Sec-Fetch-Site') == 'cross-site':
                raise ValueError('Cross-site capture is forbidden')
            url = urlsplit(self.path)
            if url.path not in ('/__start', '/__frame', '/__done') or url.fragment:
                raise ValueError('Unknown capture route')
            query = parse_qs(url.query, keep_blank_values=True, strict_parsing=True)
            expected = {'cut'} if url.path == '/__start' else {'cut', 'token'}
            if url.path == '/__frame':
                expected.add('frame')
            if set(query) != expected or any(len(v) != 1 for v in query.values()):
                raise ValueError('Invalid capture query')
            values = {key: value[0] for key, value in query.items()}
            if values['cut'] != 'pitch':
                raise ValueError('Unknown capture cut')
            return url.path, values
        def capture_body(self, frame):
            lengths = self.headers.get_all('Content-Length', [])
            if len(lengths) != 1 or not lengths[0].isascii() or not lengths[0].isdigit():
                raise ValueError('Missing or invalid content length')
            length = int(lengths[0])
            if self.headers.get('Transfer-Encoding') or length > MAX_JPEG_BYTES:
                raise ValueError('Capture body is too large or chunked')
            if frame and self.headers.get('Content-Type', '').lower() != 'image/jpeg':
                raise ValueError('Expected image/jpeg')
            if not frame and length:
                raise ValueError('Unexpected capture request body')
            self.connection.settimeout(30)
            data = self.rfile.read(length)
            if len(data) != length:
                raise ValueError('Incomplete capture request body')
            return data
        def translate_path(self, raw):
            path = unquote(urlsplit(raw).path)
            prefix = '/vendor/three/'
            base, relative = (three, path[len(prefix):]) if path.startswith(prefix) else (ROOT, path.lstrip('/'))
            result = (base / relative).resolve()
            if not result.is_relative_to(base):
                raise PermissionError('Path escapes its served directory')
            return str(result)
        def send_head(self):
            try:
                self.translate_path(self.path)
            except PermissionError:
                self.send_error(403, 'Path outside served directories')
                return None
            return super().send_head()
        def end_headers(self):
            self.send_header('Cache-Control', 'no-store')
            super().end_headers()
    return Handler

if __name__ == '__main__':
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8770)
    parser.add_argument('--three', type=Path, default=ROOT / 'viewer/vendor/three')
    args = parser.parse_args()
    three = args.three.resolve()
    try:
        package = json.loads((three / 'package.json').read_text())
        assert package['name'] == 'three' and package['version'] == '0.163.0'
        assert (three / 'examples/jsm/postprocessing/BokehPass.js').is_file()
    except (OSError, ValueError, KeyError, AssertionError):
        parser.error('--three must contain the required browser modules from three.js 0.163.0')
    print(f'Viewer: http://127.0.0.1:{args.port}/viewer/index.html?trace=YOUR_TRACE_URL', flush=True)
    print(f'Local three.js: {three}', flush=True)
    ThreadingHTTPServer(('127.0.0.1', args.port), handler_for(three)).serve_forever()
