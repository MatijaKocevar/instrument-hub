from __future__ import annotations

import os
import re
import signal
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .comment import html_to_text
from .log import log


ACTIVE: subprocess.Popen[str] | None = None
SESSION_TIMEOUT = 3 * 60 * 60

NESTED_ENV_BLOCK = {
    "OPENCODE_SESSION_ID",
    "OPENCODE",
    "OPENCODE_TERMINAL",
    "OPENCODE_DISABLE_CHANNEL_DB",
}


@dataclass(frozen=True)
class SessionResult:
    status: str
    summary: str = ""
    verification: str = ""
    reason: str = ""
    needed: str = ""


def stop_active_session(signum: int, _frame: Any) -> None:
    process = ACTIVE
    if process is not None and process.poll() is None:
        try:
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
    raise SystemExit(128 + signum)


def build_brief(
    *,
    task: dict[str, Any],
    repo: Path,
    branch: str,
    result_path: Path,
    attempt: int,
    prior_note: str,
) -> str:
    title = str(task.get("title", "Untitled task"))
    description = html_to_text(str(task.get("description", "")))
    task_id = str(task.get("id", ""))

    lines = [
        f"# Autopilot task: {title}",
        "",
        f"Repository: {repo}",
        f"Time Manager task id: {task_id}",
        f"Branch: {branch}",
        f"Attempt: {attempt}",
        "",
        "This is a single-task session. Do only this task. Do not start the next task.",
        "You are already on the task branch. Do not create, switch, merge, rebase or delete branches.",
        "Do not run git push. Do not commit unless the task truly cannot proceed without a checkpoint.",
        "The autopilot driver owns commits, merges and Time Manager updates.",
        "",
        "## Task",
        "",
        description,
        "",
        "## How to work",
        "",
        "1. Research the repository and the affected code before editing.",
        "2. Implement the task end to end. Follow AGENTS.md and the locked decisions.",
        "3. Build only the affected target with an explicit --target. Never build all.",
        "4. Restart the app once the change is ready, detached, and leave it running.",
        "5. Verify with Playwright against the running UI when the task affects UI behavior.",
        "   Use the Playwright MCP tools, not new test files or test dependencies.",
        "   If no UI is running, start the debug app with scripts/dev.sh detached first.",
        "6. Testing is disabled: do not create test files, test targets, test runners or test tooling.",
        "7. Keep verification artifacts outside the repo, under /tmp/opencode.",
        "",
        "## Result file",
        "",
        f"Write the result to: {result_path}",
        "",
        "Use exactly one of these formats:",
        "",
        "STATUS: done",
        "SUMMARY: one or two lines about what changed",
        "VERIFICATION: what you ran, including Playwright checks and their outcome",
        "",
        "STATUS: blocked",
        "REASON: what stopped you",
        "NEEDED: what a human must do",
        "",
        "STATUS: partial",
        "REASON: how far you got and what remains",
        "",
        "Never ask questions. If you cannot finish, write blocked or partial and stop.",
    ]

    if attempt > 1:
        lines += [
            "",
            "## Attempt history",
            "",
            f"This is attempt {attempt}. The previous attempt did not finish: {prior_note}",
            "Nothing was reverted. Inspect git status and git diff, verify the existing work,",
            "then finish it instead of starting over unless the tree is wrong.",
        ]

    return "\n".join(lines) + "\n"


def run_session(
    *,
    repo: Path,
    model: str,
    agent: str,
    serve_url: str,
    title: str,
    brief_path: Path,
    result_path: Path,
    log_path: Path,
) -> tuple[int, bool]:
    result_path.parent.mkdir(parents=True, exist_ok=True)
    result_path.unlink(missing_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    message = (
        "Execute the attached autopilot task brief exactly. "
        f"Write your result file to {result_path}. Do not ask questions."
    )

    command = [
        "opencode",
        "run",
        "--auto",
        "--agent",
        agent,
        "--title",
        title[:80],
        "-m",
        model,
        "--file",
        str(brief_path),
        message,
    ]
    if serve_url:
        command += ["--server", serve_url]

    env = {
        key: value for key, value in os.environ.items() if key not in NESTED_ENV_BLOCK
    }
    env["PWD"] = str(repo)

    with log_path.open("w") as log_handle:
        process = subprocess.Popen(
            command,
            cwd=str(repo),
            env=env,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            text=True,
            start_new_session=True,
        )

        global ACTIVE
        ACTIVE = process
        timed_out = False
        try:
            process.wait(timeout=SESSION_TIMEOUT)
            return process.returncode, timed_out
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            return -1, timed_out
        finally:
            ACTIVE = None


def parse_result(path: Path) -> SessionResult | None:
    if not path.exists():
        return None

    text = path.read_text(errors="replace")
    status_match = re.search(
        r"^\s*STATUS:\s*(done|blocked|partial)\s*$", text, re.M | re.I
    )
    if status_match is None:
        return None

    fields: dict[str, str] = {}
    current: str | None = None
    for line in text.splitlines():
        match = re.match(
            r"^\s*(STATUS|SUMMARY|VERIFICATION|REASON|NEEDED):\s*(.*)$",
            line,
            re.I,
        )
        if match is not None:
            current = match.group(1).upper()
            fields[current] = match.group(2).strip()
        elif current is not None and line.strip():
            fields[current] = f"{fields[current]} {line.strip()}".strip()

    return SessionResult(
        status=status_match.group(1).lower(),
        summary=fields.get("SUMMARY", ""),
        verification=fields.get("VERIFICATION", ""),
        reason=fields.get("REASON", ""),
        needed=fields.get("NEEDED", ""),
    )


def log_tail(path: Path, lines: int = 12, width: int = 180) -> str:
    if not path.exists():
        return ""
    try:
        content = path.read_text(errors="replace").splitlines()
    except OSError:
        return ""
    return "\n".join(line[:width] for line in content[-lines:])
