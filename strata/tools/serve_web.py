#!/usr/bin/env python3
"""Serves the web build for local testing (phones on the same network too).

Usage: serve_web.py [directory] [--port 8080] [--host ADDRESS] [--no-isolation]

Sends the cross-origin isolation headers (COOP/COEP) that the multi-threaded
build (strata-mt.js) needs, the right MIME type for .wasm, and no-cache headers
so a rebuild shows up on reload. The page sends its log here when it is
served from localhost, so messages from a phone appear in this terminal. Listens on all interfaces by default: open
http://<this computer's address>:8080 on a phone to try the touch controls
(service workers and fullscreen need https or localhost, but the game itself
runs over plain http). --host 127.0.0.1 keeps it to this device (Termux).
"""
import argparse
import functools
import http.server
import socketserver


class Handler(http.server.SimpleHTTPRequestHandler):
    isolate = True
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".webmanifest": "application/manifest+json",
        ".svg": "image/svg+xml",
    }

    def do_POST(self):
        # The game's log, mirrored by index.html (navigator.sendBeacon).
        if self.path.split("?")[0].endswith("/__strata/log"):
            length = int(self.headers.get("Content-Length") or 0)
            text = self.rfile.read(min(length, 8192)).decode("utf-8", "replace")
            print(f"[page] {text}", flush=True)
            self.send_response(204)
            self.end_headers()
            return
        self.send_error(404)

    def log_request(self, code="-", size="-"):
        if "/__strata/log" not in self.path:
            super().log_request(code, size)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        if self.isolate:
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("directory", nargs="?", default=".", help="folder with index.html and strata.js")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--host", default="", help="address to listen on (default: all interfaces)")
    parser.add_argument("--no-isolation", action="store_true", help="omit COOP/COEP (single-threaded build only)")
    args = parser.parse_args()
    Handler.isolate = not args.no_isolation
    handler = functools.partial(Handler, directory=args.directory)
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer((args.host, args.port), handler) as httpd:
        print(f"Serving {args.directory} on http://localhost:{args.port}/ (isolation {'on' if Handler.isolate else 'off'})",
              flush=True)
        httpd.serve_forever()


if __name__ == "__main__":
    main()
