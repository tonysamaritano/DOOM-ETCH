#!/usr/bin/env python3
"""Read-only localhost viewer server; Python is outside the drawing path."""
import argparse
import json
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

def handler_for(three):
    class Handler(SimpleHTTPRequestHandler):
        extensions_map = {**SimpleHTTPRequestHandler.extensions_map,
                          '.js': 'text/javascript', '.glb': 'model/gltf-binary'}
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
