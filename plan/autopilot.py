#!/usr/bin/env python3
"""
Instrument Hub autopilot driver.

Runs one small `opencode run` session per task from ROADMAP.md, in dependency
order, only during DeepSeek off-peak hours. A blocked task never stops the run.

Usage:
    python3 autopilot.py --dry-run          # show the queue, change nothing
    python3 autopilot.py                    # run until nothing runnable remains
    python3 autopilot.py --max-sessions 3
    python3 autopilot.py --ignore-schedule  # run now, even at peak price

Env overrides: IH_REPO, IH_MODEL, IH_AGENT, IH_SERVE_URL
(Namespaced on purpose — plain AGENT/MODEL/REPO collide with harness env.)
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import time
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

# ---------------------------------------------------------------- config

# The ledger, roadmap and blockers live in the repo that owns this script.
# A run once drifted for 23 tasks because REPO was hardcoded while PLAN_DIR was
# derived from __file__: sessions and commits landed in the repo, STATE.json in
# the copy's folder. Keep both derived from the same place. IH_REPO still wins
# for the rare case of driving a checkout from elsewhere.
_SCRIPT_REPO = Path(__file__).resolve().parent.parent
REPO = Path(os.environ.get("IH_REPO", _SCRIPT_REPO)).resolve()
PLAN_DIR = Path(os.environ.get("IH_PLAN", REPO / "plan")).resolve()
MODEL = os.environ.get("IH_MODEL", "deepseek/deepseek-flash")
AGENT = os.environ.get("IH_AGENT", "build")
SERVE_URL = os.environ.get("IH_SERVE_URL", "")  # e.g. http://localhost:4096

WORK = PLAN_DIR / ".autopilot"
LOG = WORK / "autopilot.log"
STATE = PLAN_DIR / "STATE.json"
ROADMAP = PLAN_DIR / "ROADMAP.md"
BLOCKERS = PLAN_DIR / "BLOCKERS.md"

# The child session currently running, so a Ctrl-C or SIGTERM takes its whole
# process group down instead of leaving it editing the repo.
ACTIVE: subprocess.Popen | None = None

PEAK_BLOCKS = [(1, 4), (6, 10)]  # UTC hours, Monday-Friday
HOLIDAYS: set[str] = set()  # 'YYYY-MM-DD', add Chinese holidays here
PEAK_MARGIN = timedelta(minutes=20)
SESSION_TIMEOUT = 3 * 60 * 60  # seconds

MAX_ATTEMPTS = 3
# Seconds to wait before attempt 2, 3, 4. A session that dies on a transport
# error has not judged the task, so retrying it immediately just re-runs the
# same work against an endpoint that is still flapping.
RETRY_BACKOFF = [60, 300, 900]

# Inherited from the parent agent; must not leak into a nested run.
NESTED_ENV_BLOCK = {
    "OPENCODE_SESSION_ID",
    "OPENCODE",
    "OPENCODE_TERMINAL",
    "OPENCODE_DISABLE_CHANNEL_DB",
}

RESULT_PROMPT = """Execute the attached task brief exactly. Work only on it.

When finished, write the result file at:
  {result}

Use exactly one of:
  STATUS: done      (+ SUMMARY: and EVIDENCE: lines)
  STATUS: blocked   (+ REASON: and NEEDED: lines)
  STATUS: partial   (+ REASON: line)

Never ask questions; nobody is watching. If truly stuck, write blocked and stop.
"""

# ---------------------------------------------------------------- helpers


def log(msg: str) -> None:
    line = f"{datetime.now(timezone.utc):%Y-%m-%dT%H:%M:%SZ}  {msg}"
    print(line, flush=True)
    WORK.mkdir(parents=True, exist_ok=True)
    with LOG.open("a") as fh:
        fh.write(line + "\n")


def is_peak(dt: datetime) -> bool:
    d = dt.astimezone(timezone.utc)
    if d.weekday() >= 5:
        return False
    if d.date().isoformat() in HOLIDAYS:
        return False
    return any(a <= d.hour < b for a, b in PEAK_BLOCKS)


def next_offpeak(dt: datetime) -> datetime:
    d = dt.astimezone(timezone.utc).replace(second=0, microsecond=0)
    for _ in range(60 * 24 * 8):
        if not is_peak(d):
            return d
        d += timedelta(minutes=1)
    return d


def next_safe_start(dt: datetime) -> datetime:
    """Earliest moment a session may start: off-peak and not within
    PEAK_MARGIN of a peak block beginning."""
    d = dt.astimezone(timezone.utc).replace(second=0, microsecond=0)
    if is_peak(d):
        for start, end in PEAK_BLOCKS:
            if start <= d.hour < end:
                return d.replace(hour=end, minute=0)
    if too_close_to_peak(d):
        for start, end in PEAK_BLOCKS:
            boundary = d.replace(hour=start, minute=0)
            if timedelta(0) <= boundary - d <= PEAK_MARGIN:
                return d.replace(hour=end, minute=0)
    return d


def too_close_to_peak(dt: datetime) -> bool:
    d = dt.astimezone(timezone.utc)
    if d.weekday() >= 5:
        return False
    for start, _ in PEAK_BLOCKS:
        boundary = d.replace(hour=start, minute=0, second=0, microsecond=0)
        if timedelta(0) <= boundary - d <= PEAK_MARGIN:
            return True
    return False


def human_delta(seconds: float) -> str:
    m = int(seconds // 60)
    return f"{m // 60}h{m % 60:02d}m" if m >= 60 else f"{m}m"


ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")


def plain_lines(text: str) -> list[str]:
    """Session output without colour codes, blank lines already dropped."""
    out = (ANSI_RE.sub("", line).rstrip() for line in (text or "").splitlines())
    return [line for line in out if line.strip()]


def tail_lines(path: Path, n: int, width: int = 180) -> list[str]:
    if not path.exists():
        return []
    try:
        return [
            line[:width] for line in plain_lines(path.read_text(errors="replace"))[-n:]
        ]
    except OSError:
        return []


def last_error(output: str) -> str:
    """The line a human would point at when a session died: the last one
    that looks like an error, else the last line it managed to print."""
    lines = plain_lines(output)
    for line in reversed(lines):
        if re.search(
            r"(?i)\b(error|failed|failure|exception|traceback|econn\w*|timeout|aborted)\b",
            line,
        ):
            return line[:180]
    return lines[-1][:180] if lines else "no output"


def stop_active_session(signum: int, _frame) -> None:
    """SIGINT/SIGTERM must not leave a session working on the repo."""
    proc = ACTIVE
    if proc is not None and proc.poll() is None:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
    raise SystemExit(128 + signum)


# ---------------------------------------------------------------- roadmap


def parse_roadmap() -> list[dict]:
    text = ROADMAP.read_text()
    tasks: list[dict] = []
    for block in re.split(r"^## ", text, flags=re.M)[1:]:
        head, _, body = block.partition("\n")
        m = re.match(r"(T\d{3})\s*·\s*(.+)", head.strip())
        if not m:
            continue
        task = {"id": m.group(1), "title": m.group(2).strip(), "raw": "## " + block}

        def field(key: str) -> str:
            fm = re.search(rf"^- {key}:\s*(.+)$", body, flags=re.M)
            return fm.group(1).strip() if fm else ""

        deps = field("depends")
        task["deps"] = (
            []
            if deps in ("", "none")
            else [d.strip() for d in deps.split(",") if d.strip()]
        )
        task["accept"] = field("accept")
        tasks.append(task)
    return tasks


# ---------------------------------------------------------------- state


def load_state() -> dict:
    if STATE.exists():
        return json.loads(STATE.read_text())
    return {"tasks": {}}


def save_state(state: dict) -> None:
    tmp = STATE.with_suffix(".tmp")
    tmp.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n")
    tmp.replace(STATE)


def status(state: dict, tid: str) -> str:
    return state["tasks"].get(tid, {}).get("status", "todo")


def pick_next(tasks: list[dict], state: dict) -> dict | None:
    for task in tasks:
        if status(state, task["id"]) != "todo":
            continue
        if all(status(state, d) == "done" for d in task["deps"]):
            return task
    return None


def propagate_blocks(tasks: list[dict], state: dict) -> int:
    """A task whose dependency is blocked cannot run. Mark it blocked too,
    so the run stops honestly instead of building on sand."""
    count = 0
    changed = True
    while changed:
        changed = False
        for task in tasks:
            if status(state, task["id"]) != "todo":
                continue
            bad = [d for d in task["deps"] if status(state, d) == "blocked"]
            if bad:
                entry = state["tasks"].setdefault(task["id"], {"attempts": 0})
                entry["status"] = "blocked"
                entry["note"] = f"upstream blocked: {', '.join(bad)}"
                log(f"  {task['id']} blocked (upstream {', '.join(bad)})")
                count += 1
                changed = True
    return count


# ---------------------------------------------------------------- execution


def write_brief(task: dict, entry: dict) -> Path:
    tid = task["id"]
    brief = WORK / f"{tid}.md"
    text = (
        task["raw"]
        + f"\n\n---\n\nRepo: `{REPO}`\n"
        + f"Plan folder: `{PLAN_DIR}`\n\n"
        + "Read these when the task needs them:\n"
        + f"- `{PLAN_DIR / 'DECISIONS.md'}` — locked technical decisions\n"
        + f"- `{PLAN_DIR / 'TESTING.md'}` — how to verify without hardware\n"
        + f"- `{PLAN_DIR / 'BLOCKERS.md'}` — append here when blocked, never ask questions\n"
        + f"- `{PLAN_DIR / 'AUTOPILOT.md'}` — the session and blocker protocol\n\n"
        + "Do only this task. Run its acceptance check before declaring done.\n"
    )
    prior = int(entry.get("attempts", 0))
    if prior:
        # A retry is not a fresh start: the previous session's edits are still
        # in the tree. Say so, or the next session redoes the whole task and
        # the two attempts fight over the same files.
        text += (
            f"\n## Attempt history\n\n"
            f"This is attempt {prior + 1} of {MAX_ATTEMPTS}; attempt {prior} did not finish.\n"
            f"Reported: {entry.get('note') or 'the session ended without a result file'}\n\n"
            "Nothing was reverted: the previous session's edits (and untracked files)\n"
            "are still in the working tree. Begin with `git status` and `git diff`,\n"
            "then verify and finish that work. Only start over if the tree is wrong.\n"
        )
        previous = tail_lines(WORK / f"{tid}.log", 12)
        if previous:
            text += (
                "\nLast lines of the previous session, for context only:\n\n```\n"
                + "\n".join(previous)
                + "\n```\n"
            )
    brief.write_text(text)
    return brief


def append_blocker_stub(task: dict, reason: str = "") -> None:
    tid = task["id"]
    text = BLOCKERS.read_text()
    if f"task: {tid}" in text:
        return
    n = len(re.findall(r"^## B\d{3}", text, flags=re.M)) + 1
    what = reason or "task reported blocked without an explanation"
    with BLOCKERS.open("a") as fh:
        fh.write(
            f"\n## B{n:03d} · Autopilot blocked on {tid}\n"
            f"- task: {tid}\n- severity: hard\n- status: open\n"
            f"- what: {what}\n"
            f"- why: see the session log in `.autopilot/{tid}.log`\n"
            f"- human must: read the log for {tid}\n- workaround: none recorded\n"
        )
    log(f"  appended blocker stub B{n:03d} for {tid}")


def run_task(task: dict, entry: dict, attach: str) -> tuple[str, str]:
    """Run one session and say what happened: (outcome, detail).

    `done`/`blocked`/`partial` come from the result file the session wrote.
    `crashed` means the process died before writing one, which is a transport
    or tooling failure, not a verdict on the task.
    """
    tid = task["id"]
    attempt = int(entry.get("attempts", 0)) + 1
    brief = write_brief(task, entry)
    result = WORK / f"{tid}.result"
    result.unlink(missing_ok=True)

    cmd = [
        "opencode",
        "run",
        "--auto",
        "--agent",
        AGENT,
        "--title",
        f"{tid} {task['title']}"[:80],
        "-m",
        MODEL,
    ]
    if attach:
        cmd += ["--server", attach]
    # No --standalone: sessions run on the shared background service, so the
    # TUI lists them and animates their status. Trade-off: killing this CLI
    # does not stop the session server-side.
    cmd += ["--file", str(brief), RESULT_PROMPT.format(result=result)]

    # Never inherit the parent session's identity into a nested run.
    env = {k: v for k, v in os.environ.items() if k not in NESTED_ENV_BLOCK}
    # opencode resolves the project from PWD, not the process cwd.
    # Without this the session is filed under the plan folder.
    env["PWD"] = str(REPO)

    log(f"START {tid} · {task['title']} · attempt {attempt}/{MAX_ATTEMPTS}")
    started = datetime.now(timezone.utc)
    output = ""
    rc = 0
    timed_out = False
    proc = None
    global ACTIVE
    try:
        # Own process group: a timeout must take the MCP children and browsers
        # with it. An orphaned session would keep editing the repo while the
        # retry works on the same files.
        proc = subprocess.Popen(
            cmd,
            cwd=str(REPO),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            start_new_session=True,
        )
        ACTIVE = proc
        try:
            output, _ = proc.communicate(timeout=SESSION_TIMEOUT)
            rc = proc.returncode
        except subprocess.TimeoutExpired:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            output, _ = proc.communicate()
            log(f"TIMEOUT {tid} after {SESSION_TIMEOUT}s (process group killed)")
            rc = -1
            timed_out = True
    except FileNotFoundError:
        log("FATAL: `opencode` not found on PATH")
        sys.exit(2)
    finally:
        ACTIVE = None

    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / f"{tid}.log").write_text(output or "")

    elapsed = int((datetime.now(timezone.utc) - started).total_seconds())
    log(f"END   {tid} rc={rc} in {human_delta(elapsed)}")

    body = result.read_text().strip() if result.exists() else ""
    m = re.search(r"^\s*STATUS:\s*(done|blocked|partial)\s*$", body, flags=re.M | re.I)
    if m:
        return m.group(1).lower(), ""

    if timed_out:
        return "crashed", f"timeout after {SESSION_TIMEOUT}s (process group killed)"

    detail = last_error(output)
    if rc != 0:
        log(f"CRASH {tid} rc={rc} before a result file was written: {detail}")
        return "crashed", detail
    log(f"PARTIAL {tid} exited cleanly but wrote no usable result: {detail}")
    return "partial", "session exited without a result file"


def git_commit(task: dict) -> None:
    if not (REPO / ".git").exists():
        return
    try:
        subprocess.run(["git", "-C", str(REPO), "add", "-A"], check=False)
        msg = f"autopilot: {task['id']} {task['title']}"
        subprocess.run(["git", "-C", str(REPO), "commit", "-qm", msg], check=False)
        log(f"  committed {task['id']}")
    except Exception as exc:  # noqa: BLE001
        log(f"  commit skipped: {exc}")


def wait_for_offpeak(ignore: bool) -> None:
    if ignore:
        return
    now = datetime.now(timezone.utc)
    target = next_safe_start(now)
    if target <= now:
        return
    seconds = (target - now).total_seconds()
    log(f"WAIT  peak pricing until {target:%Y-%m-%d %H:%M}Z ({human_delta(seconds)})")
    while True:
        remaining = (target - datetime.now(timezone.utc)).total_seconds()
        if remaining <= 0:
            break
        time.sleep(min(remaining, 300))
    log("GO    off-peak")


# ---------------------------------------------------------------- main


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--max-sessions", type=int, default=0)
    ap.add_argument("--ignore-schedule", action="store_true")
    ap.add_argument(
        "--reset-failed",
        action="store_true",
        help="set every non-done task back to todo",
    )
    args = ap.parse_args()

    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, stop_active_session)

    WORK.mkdir(parents=True, exist_ok=True)
    tasks = parse_roadmap()
    state = load_state()

    if args.reset_failed:
        for t in tasks:
            if status(state, t["id"]) != "done":
                state["tasks"].setdefault(t["id"], {})["status"] = "todo"
                state["tasks"][t["id"]]["attempts"] = 0
                state["tasks"][t["id"]].pop("note", None)
        save_state(state)

    if state.get("_roadmap_count") != len(tasks):
        state["_roadmap_count"] = len(tasks)
        for t in tasks:
            state["tasks"].setdefault(t["id"], {"status": "todo", "attempts": 0})
        save_state(state)

    log(f"roadmap: {len(tasks)} tasks · repo: {REPO}")
    log(f"plan: {PLAN_DIR} · state: {STATE}")
    log(f"config: agent={AGENT} model={MODEL} server={SERVE_URL or 'local'}")
    done = sum(1 for t in tasks if status(state, t["id"]) == "done")
    blocked = sum(1 for t in tasks if status(state, t["id"]) == "blocked")
    log(f"state: {done} done, {blocked} blocked, {len(tasks) - done - blocked} todo")

    if not REPO.exists():
        log(f"FATAL: repo {REPO} does not exist")
        return 2
    if shutil.which("opencode") is None and not args.dry_run:
        log("FATAL: `opencode` not on PATH")
        return 2

    sessions = 0
    while True:
        if propagate_blocks(tasks, state):
            save_state(state)
        task = pick_next(tasks, state)
        if task is None:
            break
        if args.max_sessions and sessions >= args.max_sessions:
            log(f"stopping after {sessions} sessions (--max-sessions)")
            break

        if args.dry_run:
            sessions += 1
            log(f"WOULD RUN {task['id']} · {task['title']}")
            # simulate success so the rest of the graph becomes runnable
            state["tasks"].setdefault(task["id"], {})["status"] = "done"
            continue

        wait_for_offpeak(args.ignore_schedule)
        entry = state["tasks"].setdefault(task["id"], {"status": "todo", "attempts": 0})
        outcome, detail = run_task(task, entry, SERVE_URL)
        sessions += 1

        entry["attempts"] = int(entry.get("attempts", 0)) + 1
        if outcome == "done":
            entry["status"] = "done"
            entry.pop("note", None)
            # Persist before committing: the checkpoint must contain the status
            # it is checking in, or the ledger lags one task forever.
            save_state(state)
            git_commit(task)
        elif outcome == "blocked":
            entry["status"] = "blocked"
            entry["note"] = detail or "session reported blocked"
            append_blocker_stub(task, entry["note"])
            save_state(state)
            git_commit(task)
        else:
            entry["note"] = f"{outcome}: {detail}" if detail else outcome
            if entry["attempts"] >= MAX_ATTEMPTS:
                entry["status"] = "blocked"
                log(
                    f"  {task['id']} failed {entry['attempts']} times → blocked ({entry['note']})"
                )
                append_blocker_stub(task, entry["note"])
                save_state(state)
                git_commit(task)
            else:
                entry["status"] = "todo"
                save_state(state)
                delay = RETRY_BACKOFF[
                    min(entry["attempts"] - 1, len(RETRY_BACKOFF) - 1)
                ]
                log(f"RETRY {task['id']} in {human_delta(delay)} · {entry['note']}")
                time.sleep(delay)
        save_state(state)

    done = sum(1 for t in tasks if status(state, t["id"]) == "done")
    blocked = sum(1 for t in tasks if status(state, t["id"]) == "blocked")
    todo = len(tasks) - done - blocked
    log(f"FINISHED · {done} done · {blocked} blocked · {todo} todo")
    if blocked:
        log(f"see {BLOCKERS} for the human queue")
    return 0


if __name__ == "__main__":
    sys.exit(main())
