#!/usr/bin/env python3
"""Read-only autopilot status page. No dependencies, no writes."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import time
from collections import Counter
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STATE = ROOT / "plan" / "STATE.json"
ROADMAP = ROOT / "plan" / "ROADMAP.md"
LOG = ROOT / "plan" / ".autopilot" / "autopilot.log"

TASK_HEADING = re.compile(r"^## (T\d+) · (.+)$", re.M)
START_EVENT = re.compile(r"^(?P<stamp>\S+)\s+START\s+(?P<task>T\d+)\s")
END_EVENT = re.compile(
    r"^(?P<stamp>\S+)\s+END\s+(?P<task>T\d+)\s+rc=\S+\s+in\s+(?P<duration>\S+)"
)


def tail_lines(path: Path, count: int) -> list[str]:
    if not path.exists():
        return []
    try:
        return path.read_text(errors="replace").splitlines()[-count:]
    except OSError:
        return []


def task_heading_map() -> tuple[dict[str, str], list[str]]:
    try:
        text = ROADMAP.read_text(errors="replace")
    except OSError:
        return {}, []

    matches = list(TASK_HEADING.finditer(text))
    return {m.group(1): m.group(2).strip() for m in matches}, [
        m.group(1) for m in matches
    ]


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


def elapsed_since(stamp: str) -> str:
    try:
        started = datetime.strptime(stamp, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=timezone.utc
        )
    except ValueError:
        return ""

    seconds = max(0, int((datetime.now(timezone.utc) - started).total_seconds()))
    if seconds < 60:
        return f"{seconds}s"
    if seconds < 3600:
        return f"{seconds // 60}m"
    return f"{seconds // 3600}h{seconds % 3600 // 60:02d}m"


def clock(stamp: str) -> str:
    return stamp[11:16] + "Z" if len(stamp) >= 16 else stamp


RUN = ROOT / "plan" / ".autopilot" / "run.json"


def run_task_ids(order: list[str]) -> list[str]:
    """Tasks queued for the current run, newest ROADMAP additions first."""
    if RUN.exists():
        try:
            data = json.loads(RUN.read_text())
            ids = [str(tid) for tid in data.get("tasks", [])]
            if ids:
                return [tid for tid in order if tid in set(ids)] or ids
        except (OSError, ValueError):
            pass

    try:
        commits = subprocess.run(
            [
                "git",
                "-C",
                str(ROOT),
                "log",
                "-n",
                "20",
                "--format=%H",
                "--",
                "plan/ROADMAP.md",
            ],
            capture_output=True,
            text=True,
            check=False,
            timeout=2,
        ).stdout.split()

        for commit in commits:
            diff = subprocess.run(
                [
                    "git",
                    "-C",
                    str(ROOT),
                    "show",
                    "--unified=0",
                    "--format=",
                    commit,
                    "--",
                    "plan/ROADMAP.md",
                ],
                capture_output=True,
                text=True,
                check=False,
                timeout=2,
            ).stdout
            ids = re.findall(r"^\+## (T\d+)", diff, re.M)
            if ids:
                return [tid for tid in order if tid in set(ids)] or ids
    except (OSError, subprocess.SubprocessError):
        pass

    return order


def status() -> dict:
    tasks = json.loads(STATE.read_text())["tasks"]
    titles, order = task_heading_map()
    lines = tail_lines(LOG, 240)
    run_ids = run_task_ids(order)

    current_id = None
    current_stamp = None

    for line in reversed(lines):
        match = START_EVENT.match(line)
        if match is not None:
            current_id = match.group("task")
            current_stamp = match.group("stamp")
            break
        if END_EVENT.match(line) is not None:
            break

    end_info: dict[str, tuple[str, str]] = {}

    for line in lines:
        match = END_EVENT.match(line)
        if match is not None:
            end_info[match.group("task")] = (
                match.group("stamp"),
                match.group("duration"),
            )

    run_counts = Counter(tasks.get(tid, {}).get("status", "todo") for tid in run_ids)
    rows: list[dict[str, str]] = []

    for tid in run_ids:
        entry = tasks.get(tid, {})
        state_name = entry.get("status", "todo")

        if tid == current_id:
            status_name = "working"
            detail = (
                f"started {clock(current_stamp or '')} · "
                f"{elapsed_since(current_stamp or '')}"
            )
        elif state_name == "done":
            status_name = "done"
            ended = end_info.get(tid)
            detail = (
                f"finished {clock(ended[0])} · {ended[1]}"
                if ended is not None
                else "done"
            )
        elif state_name == "blocked":
            status_name = "blocked"
            detail = entry.get("note", "blocked")
        elif state_name == "partial":
            status_name = "partial"
            detail = entry.get("note", "partial")
        else:
            status_name = "todo"
            attempts = entry.get("attempts", 0)
            detail = f"attempt {attempts}/3" if attempts else "queued"

        rows.append(
            {
                "id": tid,
                "title": titles.get(tid, ""),
                "status": status_name,
                "detail": detail,
            }
        )

    done = run_counts.get("done", 0)
    total = len(run_ids)

    return {
        "done": done,
        "total": total,
        "todo": run_counts.get("todo", 0),
        "blocked": run_counts.get("blocked", 0),
        "partial": run_counts.get("partial", 0),
        "percent": round(done / total * 100) if total else 0,
        "driverPids": driver_pids(),
        "lastCommit": last_commit(),
        "rows": rows,
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
  h2 { font-size: 13px; color: #8b93a3; text-transform: uppercase;
       letter-spacing: .08em; margin: 26px 0 8px; }
  .sub { color: #8b93a3; font-size: 13px; margin-bottom: 22px; }
  .cards { display: flex; gap: 14px; flex-wrap: wrap; margin-bottom: 18px; }
  .card { background: #12151c; border: 1px solid #242a36; border-radius: 10px;
          padding: 14px 18px; min-width: 140px; }
  .card .n { font-size: 34px; font-weight: 650; line-height: 1.1; }
  .card .l { color: #8b93a3; font-size: 12px; text-transform: uppercase;
             letter-spacing: .08em; }
  .progress { position: relative; height: 34px; background: #1b202a;
              border-radius: 9px; overflow: hidden; margin: 4px 0 8px; }
  .progress .fill { height: 100%; background: linear-gradient(90deg,#16a34a,#22c55e);
                    transition: width .4s ease; }
  .progress span { position: absolute; inset: 0; display: flex; align-items: center;
                   justify-content: center; font-weight: 650; font-size: 15px;
                   color: #fff; text-shadow: 0 1px 2px #0008; }
  .legend { color: #8b93a3; font-size: 12px; margin-bottom: 4px; }
  table { width: 100%; border-collapse: collapse; background: #12151c;
          border: 1px solid #242a36; border-radius: 10px; overflow: hidden; }
  th { text-align: left; font-size: 11.5px; text-transform: uppercase;
       letter-spacing: .08em; color: #8b93a3; padding: 10px 12px;
       border-bottom: 1px solid #242a36; background: #10131a; }
  td { padding: 9px 12px; border-bottom: 1px solid #1b202a; vertical-align: top; }
  tr:last-child td { border-bottom: none; }
  tr.working td { background: #14251a; }
  tr.done td { color: #9aa4b2; }
  .st { display: inline-block; padding: 2px 8px; border-radius: 999px;
        font-size: 12px; font-weight: 650; text-transform: uppercase;
        letter-spacing: .04em; }
  .st-working { background: #14532d; color: #4ade80; }
  .st-done { background: #1f2937; color: #9ca3af; }
  .st-todo { background: #1e293b; color: #93c5fd; }
  .st-blocked { background: #7f1d1d; color: #fca5a5; }
  .st-partial { background: #78350f; color: #fcd34d; }
  .mono { font-variant-numeric: tabular-nums; }
  pre { background: #0f1218; border: 1px solid #1d232e; border-radius: 10px;
        padding: 14px; overflow: auto; max-height: 40vh; font-size: 12.5px;
        line-height: 1.5; color: #c8d1e0; }
  .ok { color: #22c55e; } .bad { color: #f87171; } .idle { color: #fbbf24; }
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

  <div class="progress"><div class="fill" id="fill" style="width:0%"></div>
    <span id="progressText">–</span></div>
  <div class="legend">green = done · blue = open · red = blocked · amber = partial</div>

  <h2>This run</h2>
  <table>
    <thead><tr><th>Task</th><th>Title</th><th>Status</th><th>Detail</th></tr></thead>
    <tbody id="rows"></tbody>
  </table>

  <h2>Recent log</h2>
  <pre id="log"></pre>

<script>
const el = (id) => document.getElementById(id);

function statusBadge(status) {
  const span = document.createElement('span');
  span.className = 'st st-' + status;
  span.textContent = status;
  return span;
}

async function tick() {
  try {
    const r = await fetch('/status.json', { cache: 'no-store' });
    const s = await r.json();

    el('done').textContent = s.done;
    el('total').textContent = s.total;
    el('todo').textContent = s.todo;
    el('blocked').textContent = s.blocked;
    el('fill').style.width = s.percent + '%';
    el('progressText').textContent = s.done + ' / ' + s.total + ' · ' + s.percent + '%';

    const running = s.driverPids.length > 0;
    el('sub').textContent =
      (running ? 'driver running (pid ' + s.driverPids.join(', ') + ')' : 'driver stopped') +
      (s.lastCommit ? ' · last commit ' + s.lastCommit : '');

    const body = el('rows');
    body.replaceChildren();
    for (const row of s.rows) {
      const tr = document.createElement('tr');
      tr.className = 'row ' + row.status;
      const id = document.createElement('td');
      id.className = 'mono';
      id.textContent = row.id;
      const title = document.createElement('td');
      title.textContent = row.title;
      const status = document.createElement('td');
      status.append(statusBadge(row.status));
      const detail = document.createElement('td');
      detail.className = 'mono';
      detail.textContent = row.detail;
      tr.append(id, title, status, detail);
      body.append(tr);
    }

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
