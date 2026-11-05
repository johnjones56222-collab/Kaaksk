#!/usr/bin/env python3
"""Serves the web build for local testing (phones on the same network too).

Usage: serve_web.py [directory] [--port 8080] [--no-isolation]

Sends the cross-origin isolation headers (COOP/COEP) that the multi-threaded
build (strata-mt.js) needs, the right MIME type for .wasm, and no-cache headers
so a rebuild shows up on reload. Listens on all interfaces: open
http://<this computer's address>:8080 on a phone to try the touch controls
(service workers and fullscreen need https or localhost, but the game itself
runs over plain http).
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
    parser.add_argument("--no-isolation", action="store_true", help="omit COOP/COEP (single-threaded build only)")
    args = parser.parse_args()
    Handler.isolate = not args.no_isolation
    handler = functools.partial(Handler, directory=args.directory)
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("", args.port), handler) as httpd:
        print(f"Serving {args.directory} on http://localhost:{args.port}/ (isolation {'on' if Handler.isolate else 'off'})")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
