"""Serve this checkout's UI prototype using Python's standard library."""

import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import urllib.request
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parent
STATE = ROOT / ".preview"


def identity(port):
    try:
        with urllib.request.urlopen(
            f"http://127.0.0.1:{port}/__preview__", timeout=0.5
        ) as response:
            result = json.load(response)
        return result if isinstance(result, dict) else None
    except (OSError, ValueError):
        return None


def owned(server):
    return server is not None and server.get("root") == str(ROOT)


class PreviewHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_GET(self):
        if urlsplit(self.path).path != "/__preview__":
            return super().do_GET()
        body = json.dumps({
            "root": str(ROOT), "pid": os.getpid(), "entry": "prototype.html"
        }).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8769)
    parser.add_argument("--stop", action="store_true")
    parser.add_argument("--serve", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")

    if args.serve:
        with ThreadingHTTPServer(("127.0.0.1", args.port), PreviewHandler) as server:
            server.serve_forever()
        return 0

    running = identity(args.port)
    if args.stop:
        if running is None:
            print("No repository preview is running on this port.")
            return 0
        if not owned(running):
            raise SystemExit("This port belongs to another preview; not stopping it.")
        os.kill(int(running["pid"]), signal.SIGTERM)
        for _ in range(50):
            if not owned(identity(args.port)):
                (STATE / f"server-{args.port}.pid").unlink(missing_ok=True)
                print("Repository preview stopped.")
                return 0
            time.sleep(0.1)
        raise SystemExit("Preview did not stop.")

    url = f"http://127.0.0.1:{args.port}/prototype.html"
    if owned(running):
        print(url)
        print("Serving:", ROOT)
        return 0
    if running is not None:
        raise SystemExit("This port belongs to another preview; use --port.")

    STATE.mkdir(exist_ok=True)
    log_path = STATE / f"server-{args.port}.log"
    detach = (
        {"creationflags": subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP}
        if os.name == "nt" else {"start_new_session": True}
    )
    with log_path.open("ab") as log:
        process = subprocess.Popen(
            [sys.executable, str(Path(__file__).resolve()), "--serve", "--port", str(args.port)],
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
            close_fds=True, **detach,
        )
    for _ in range(50):
        running = identity(args.port)
        if owned(running) and running.get("pid") == process.pid:
            (STATE / f"server-{args.port}.pid").write_text(str(process.pid), encoding="utf-8")
            print(url)
            print("Serving:", ROOT)
            return 0
        if process.poll() is not None:
            raise SystemExit(f"Preview failed to start; see {log_path}. Use --port if occupied.")
        time.sleep(0.1)
    process.terminate()
    raise SystemExit(f"Preview did not respond; see {log_path}.")


if __name__ == "__main__":
    raise SystemExit(main())
