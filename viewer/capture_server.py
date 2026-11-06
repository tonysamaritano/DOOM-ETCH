#!/usr/bin/env python3
"""Read-only localhost viewer server; Python is outside the drawing path."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]

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
    parser.add_argument('--three', type=Path, default=ROOT / 'build/vendor/three')
    args = parser.parse_args()
    three = args.three.resolve()
    try:
        package = json.loads((three / 'package.json').read_text())
        assert package['name'] == 'three' and package['version'] == '0.163.0'
        assert (three / 'examples/jsm/postprocessing/BokehPass.js').is_file()
    except (OSError, ValueError, KeyError, AssertionError):
        parser.error('--three must point to the complete local three.js 0.163.0 package')
    print(f'Viewer: http://127.0.0.1:{args.port}/viewer/index.html?trace=YOUR_TRACE_URL', flush=True)
    print(f'Local three.js: {three}', flush=True)
    ThreadingHTTPServer(('127.0.0.1', args.port), handler_for(three)).serve_forever()
