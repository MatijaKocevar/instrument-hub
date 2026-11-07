#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLAN = ROOT / "plan"
ROADMAP = PLAN / "ROADMAP.md"
STATE = PLAN / "STATE.json"
LOG = PLAN / ".autopilot" / "autopilot.log"
HTML = Path(__file__).with_suffix(".html")
TASK = re.compile(r"(T\d{3})\s*·\s*(.+)")
START = re.compile(r"^(\S+)\s+START\s+(T\d{3})")
END = re.compile(r"^(\S+)\s+END\s+(T\d{3})")


def field(body: str, key: str) -> str:
    match = re.search(rf"^- {key}:\s*(.+)$", body, flags=re.M)
    return match.group(1).strip() if match else ""


def read_tasks() -> list[dict]:
    tasks = []
    text = ROADMAP.read_text()
    for block in re.split(r"^## ", text, flags=re.M)[1:]:
        head, _, body = block.partition("\n")
        match = TASK.match(head.strip())
        if match is None:
            continue
        goal = re.sub(r"\s+", " ", field(body, "goal"))
        if len(goal) > 240:
            goal = goal[:237] + "..."
        tasks.append(
            {
                "id": match.group(1),
                "title": match.group(2).strip(),
                "goal": goal,
                "size": field(body, "size"),
                "depends": field(body, "depends"),
            }
        )
    return tasks


def read_state() -> dict:
    try:
        return json.loads(STATE.read_text())
    except (OSError, json.JSONDecodeError):
        return {"tasks": {}}


def read_log() -> tuple[list[str], str | None, str | None]:
    try:
        lines = LOG.read_text(errors="replace").splitlines()[-400:]
    except OSError:
        return [], None, None

    current = None
    started = None
    for line in lines:
        match = START.match(line)
        if match:
            current = match.group(2)
            started = match.group(1)
            continue
        match = END.match(line)
        if match and match.group(2) == current:
            current = None
            started = None
    return lines[-18:], current, started


def autopilot_running() -> bool:
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            parts = (entry / "cmdline").read_bytes().split(b"\0")
        except OSError:
            continue
        args = [part.decode(errors="replace") for part in parts if part]
        if (
            len(args) >= 2
            and "python" in Path(args[0]).name
            and args[1].endswith("plan/autopilot.py")
        ):
            return True
    return False


def elapsed(started: str | None) -> int | None:
    if not started:
        return None
    try:
        value = datetime.strptime(started, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=timezone.utc
        )
        return max(0, int((datetime.now(timezone.utc) - value).total_seconds()))
    except ValueError:
        return None


def build_status() -> dict:
    tasks = read_tasks()
    state = read_state().get("tasks", {})
    log, current, started = read_log()
    running = autopilot_running()
    rows = []
    counts = {"done": 0, "todo": 0, "blocked": 0, "working": 0}

    for task in tasks:
        entry = state.get(task["id"], {})
        status = entry.get("status", "todo")
        if status == "todo" and task["id"] == current:
            status = "working"
        if status in counts:
            counts[status] += 1
        rows.append(
            {
                **task,
                "status": status,
                "attempts": int(entry.get("attempts", 0)),
                "note": entry.get("note", ""),
                "current": task["id"] == current,
            }
        )

    total = len(rows)
    counts["total"] = total
    counts["remaining"] = total - counts["done"]
    counts["percent"] = round(counts["done"] * 100.0 / total, 1) if total else 0.0

    current_row = next((row for row in rows if row["current"]), None)
    return {
        "running": running,
        "current": (
            {
                "id": current_row["id"],
                "title": current_row["title"],
                "goal": current_row["goal"],
                "started": started,
                "elapsedSeconds": elapsed(started),
                "attempts": current_row["attempts"],
            }
            if current_row
            else None
        ),
        "counts": counts,
        "tasks": rows,
        "log": log,
        "updated": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    }


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path in ("/", "/index.html"):
            body = HTML.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path == "/api/status":
            body = json.dumps(build_status()).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_error(404)

    def log_message(self, *_args: object) -> None:
        pass


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8099)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"autopilot progress: http://{args.host}:{args.port}/", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
