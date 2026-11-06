#!/usr/bin/env python3
"""Read-only localhost viewer server; Python is outside the drawing path."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent

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
