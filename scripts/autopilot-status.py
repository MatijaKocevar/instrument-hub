#!/usr/bin/env python3
"""Read-only autopilot status page. No dependencies, no writes."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import time
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STATE = ROOT / "plan" / "STATE.json"
LOG = ROOT / "plan" / ".autopilot" / "autopilot.log"
EVENT = re.compile(
    r"^(?P<stamp>\S+)\s+(?P<kind>START|END)\s+(?P<task>T\d+)\s+(?P<text>.*)$"
)


def tail_lines(path: Path, count: int) -> list[str]:
    if not path.exists():
        return []
    try:
        return path.read_text(errors="replace").splitlines()[-count:]
    except OSError:
        return []


def driver_pids() -> list[int]:
    try:
        out = subprocess.run(
            ["pgrep", "-f", "plan/autopilot.py"],
            capture_output=True,
            text=True,
            check=False,
            timeout=2,
        ).stdout
    except (OSError, subprocess.SubprocessError):
        return []
    return [int(line) for line in out.split() if line.isdigit()]


def last_commit() -> str:
    try:
        return subprocess.run(
            ["git", "-C", str(ROOT), "log", "-1", "--format=%h %s"],
            capture_output=True,
            text=True,
            check=False,
            timeout=2,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def status() -> dict:
    tasks = json.loads(STATE.read_text())["tasks"]
    counts = Counter(entry.get("status", "todo") for entry in tasks.values())
    lines = tail_lines(LOG, 80)

    current = None
    current_since = None
    for line in reversed(lines):
        match = EVENT.match(line)
        if match is None:
            continue
        if match.group("kind") == "END":
            break
        title = match.group("text").strip(" ·").split(" · ")[0]
        current = f"{match.group('task')} · {title}"
        current_since = match.group("stamp")
        break

    todo = sorted(tid for tid, entry in tasks.items() if entry.get("status") == "todo")
    log_age = None
    if LOG.exists():
        log_age = round(time.time() - LOG.stat().st_mtime)

    return {
        "done": counts.get("done", 0),
        "total": len(tasks),
        "todo": counts.get("todo", 0),
        "blocked": counts.get("blocked", 0),
        "partial": counts.get("partial", 0),
        "current": current,
        "currentSince": current_since,
        "driverPids": driver_pids(),
        "logAgeSeconds": log_age,
        "lastCommit": last_commit(),
        "todoIds": todo,
        "log": lines[-22:],
    }


HTML = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Autopilot</title>
<style>
  :root { color-scheme: dark; }
  body { margin: 0; padding: 24px; background: #0b0c10; color: #e8edf5;
         font: 16px/1.45 system-ui, -apple-system, sans-serif; }
  h1 { margin: 0 0 4px; font-size: 22px; font-weight: 600; }
  .sub { color: #8b93a3; font-size: 13px; margin-bottom: 22px; }
  .cards { display: flex; gap: 14px; flex-wrap: wrap; margin-bottom: 20px; }
  .card { background: #12151c; border: 1px solid #242a36; border-radius: 10px;
          padding: 14px 18px; min-width: 150px; }
  .card .n { font-size: 34px; font-weight: 650; line-height: 1.1; }
  .card .l { color: #8b93a3; font-size: 12px; text-transform: uppercase;
             letter-spacing: .08em; }
  .bar { height: 16px; background: #1b202a; border-radius: 8px; overflow: hidden;
         margin: 6px 0 20px; }
  .fill { height: 100%; background: #22c55e; transition: width .4s ease; }
  .now { background: #12151c; border: 1px solid #242a36; border-radius: 10px;
         padding: 16px 18px; margin-bottom: 20px; }
  .now .task { font-size: 20px; font-weight: 600; margin-top: 4px; }
  .now .meta { color: #8b93a3; font-size: 13px; margin-top: 4px; }
  .ok { color: #22c55e; } .bad { color: #f87171; } .idle { color: #fbbf24; }
  pre { background: #0f1218; border: 1px solid #1d232e; border-radius: 10px;
        padding: 14px; overflow: auto; max-height: 46vh; font-size: 12.5px;
        line-height: 1.5; color: #c8d1e0; }
  .todo { color: #8b93a3; font-size: 13px; }
</style>
</head>
<body>
  <h1>Instrument Hub · Autopilot</h1>
  <div class="sub" id="sub">connecting…</div>
  <div class="cards">
    <div class="card"><div class="n ok" id="done">–</div><div class="l">done</div></div>
    <div class="card"><div class="n" id="total">–</div><div class="l">total</div></div>
    <div class="card"><div class="n idle" id="todo">–</div><div class="l">todo</div></div>
    <div class="card"><div class="n bad" id="blocked">–</div><div class="l">blocked</div></div>
  </div>
  <div class="bar"><div class="fill" id="fill" style="width:0%"></div></div>
  <div class="now">
    <div class="l">Current task</div>
    <div class="task" id="current">idle</div>
    <div class="meta" id="currentMeta"></div>
  </div>
  <div class="todo" id="todoList"></div>
  <h2 style="font-size:14px;color:#8b93a3;margin:22px 0 8px;">Recent log</h2>
  <pre id="log"></pre>
<script>
const el = (id) => document.getElementById(id);
const pct = (done, total) => total ? Math.round((done / total) * 100) : 0;

async function tick() {
  try {
    const r = await fetch('/status.json', { cache: 'no-store' });
    const s = await r.json();
    el('done').textContent = s.done;
    el('total').textContent = s.total;
    el('todo').textContent = s.todo;
    el('blocked').textContent = s.blocked;
    el('fill').style.width = pct(s.done, s.total) + '%';

    const running = s.driverPids.length > 0;
    el('sub').textContent =
      (running ? 'driver running (pid ' + s.driverPids.join(', ') + ')' : 'driver stopped') +
      (s.lastCommit ? ' · last commit ' + s.lastCommit : '');

    el('current').textContent = s.current || 'idle';
    el('currentMeta').textContent = s.current
      ? 'started ' + s.currentSince
      : (s.logAgeSeconds !== null ? 'log quiet for ' + s.logAgeSeconds + 's' : 'no log yet');

    el('todoList').textContent = s.todoIds.length
      ? 'next: ' + s.todoIds.slice(0, 12).join('  ·  ')
      : '';

    el('log').textContent = s.log.join('\\n');
  } catch (err) {
    el('sub').textContent = 'status unavailable: ' + err;
  }
}
tick();
setInterval(tick, 3000);
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:  # noqa: N802
        if self.path.startswith("/status.json"):
            body = json.dumps(status()).encode()
            content_type = "application/json"
        elif self.path in ("/", "/index.html"):
            body = HTML.encode()
            content_type = "text/html; charset=utf-8"
        else:
            self.send_error(404)
            return

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args: object) -> None:
        pass


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8099)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"autopilot status: http://{args.host}:{args.port}/", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
