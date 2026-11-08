from __future__ import annotations

import os
import shutil
import time
from pathlib import Path
from typing import Any

from .comment import build_comment, with_comment
from .config import AutopilotConfig
from .git_ops import (
    GitError,
    commit_all,
    current_branch,
    ensure_clean_main,
    merge_to_main,
    return_to_main,
    start_branch,
)
from .log import log
from .schedule import wait_for_offpeak
from .session import (
    build_brief,
    log_tail,
    parse_result,
    run_session,
)
from .tm import (
    TimeManagerClient,
    TimeManagerError,
    sort_tasks,
    task_branch_slug,
    task_label,
)


MAX_ATTEMPTS = 3
RETRY_BACKOFF = (60, 300, 900)


class AutopilotLock:
    def __init__(self, path: Path) -> None:
        self.path = path

    def acquire(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)

        if self.path.exists():
            try:
                pid = int(self.path.read_text().strip())
            except (OSError, ValueError):
                pid = 0

            if pid > 0 and _pid_alive(pid):
                raise RuntimeError(f"autopilot is already running with pid {pid}")

            self.path.unlink(missing_ok=True)

        self.path.write_text(str(os.getpid()))

    def release(self) -> None:
        try:
            if self.path.read_text().strip() == str(os.getpid()):
                self.path.unlink(missing_ok=True)
        except OSError:
            pass


def _pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


class AutopilotRunner:
    def __init__(self, config: AutopilotConfig) -> None:
        self.config = config
        self.tm = TimeManagerClient(config.tm)
        self.list_id = ""
        self.sessions = 0

    def run(
        self,
        *,
        dry_run: bool,
        max_sessions: int,
        once: bool,
        ignore_schedule: bool,
        margin_minutes: int,
    ) -> int:
        if shutil.which("opencode") is None:
            log("ERROR opencode is not on PATH")
            return 2

        if not dry_run:
            try:
                ensure_clean_main(self.config.repo, self.config.base_branch)
            except GitError as error:
                log(f"ERROR {error}")
                return 2

        self.list_id = self._resolve_list_id(wait=not dry_run)
        log(
            f"autopilot: repo={self.config.repo} list={self.config.tm.list_name} "
            f"model={self.config.model} agent={self.config.agent}"
        )

        if dry_run:
            return self.dry_run()

        limit = 1 if once else max_sessions
        while True:
            tasks = self._todo_tasks(wait=True)
            if not tasks:
                log("FINISHED no TODO tasks left")
                return 0

            if limit and self.sessions >= limit:
                log(f"STOP after {self.sessions} session(s) (limit)")
                return 0

            task = tasks[0]
            wait_for_offpeak(ignore=ignore_schedule, margin_minutes=margin_minutes)

            try:
                self._run_task(task)
            except Exception as error:  # noqa: BLE001
                log(f"ERROR task {task_label(task)} failed unexpectedly: {error}")
                branch = task_branch_slug(task)
                commit = ""
                try:
                    if current_branch(self.config.repo) == branch:
                        commit = self._preserve_work(task, branch)
                except GitError as git_error:
                    log(f"WARN could not checkpoint {branch}: {git_error}")

                self._finalize_on_hold(
                    task,
                    branch=branch,
                    reason=f"autopilot error: {error}",
                    needed="Inspect .autopilot logs and the task branch",
                    commit=commit,
                )

            self.sessions += 1

    def dry_run(self) -> int:
        tasks = sort_tasks(self._todo_tasks())
        if not tasks:
            log("DRY RUN no TODO tasks")
            return 0

        for task in tasks:
            log(f"WOULD RUN {task_label(task)} on {task_branch_slug(task)}")
        return 0

    def _resolve_list_id(self, *, wait: bool) -> str:
        while True:
            try:
                return self.tm.resolve_list_id()
            except TimeManagerError as error:
                if not wait:
                    raise
                log(f"TM list lookup failed: {error}")
                time.sleep(60)

    def _todo_tasks(self, *, wait: bool = False) -> list[dict[str, Any]]:
        while True:
            try:
                tasks = self.tm.list_tasks(self.list_id, status="TODO")
                return sort_tasks(tasks)
            except TimeManagerError as error:
                if not wait:
                    raise
                log(f"TM task lookup failed: {error}")
                time.sleep(60)

    def _run_task(self, task: dict[str, Any]) -> None:
        branch = task_branch_slug(task)
        resumed = start_branch(self.config.repo, branch, self.config.base_branch)
        log(
            f"START {task_label(task)} branch={branch}"
            f"{' (resumed existing branch)' if resumed else ''}"
        )
        self._set_status(task, "IN_PROGRESS")

        prior_note = ""
        for attempt in range(1, MAX_ATTEMPTS + 1):
            brief_path = self.config.work / "briefs" / f"{_safe_name(task)}.md"
            result_path = self.config.work / "results" / f"{_safe_name(task)}.result"
            log_path = self.config.work / "logs" / f"{_safe_name(task)}.log"

            brief_path.parent.mkdir(parents=True, exist_ok=True)
            brief_path.write_text(
                build_brief(
                    task=task,
                    repo=self.config.repo,
                    branch=branch,
                    result_path=result_path.resolve(),
                    attempt=attempt,
                    prior_note=prior_note,
                )
            )

            rc, timed_out = run_session(
                repo=self.config.repo,
                model=self.config.model,
                agent=self.config.agent,
                serve_url=self.config.serve_url,
                title=task_label(task),
                brief_path=brief_path,
                result_path=result_path,
                log_path=log_path,
            )

            result = parse_result(result_path)
            if result is None:
                detail = log_tail(log_path)
                if timed_out:
                    prior_note = "session timed out and was killed"
                elif rc != 0:
                    prior_note = f"session crashed with rc={rc}: {detail}"
                else:
                    prior_note = f"session exited without a result file: {detail}"
                log(f"RETRY {task_label(task)} attempt {attempt}: {prior_note}")

                if attempt < MAX_ATTEMPTS:
                    time.sleep(RETRY_BACKOFF[min(attempt - 1, len(RETRY_BACKOFF) - 1)])
                    continue

                self._finalize_on_hold(
                    task,
                    branch=branch,
                    reason=prior_note,
                    needed="Inspect the task log and branch, then move the task back to TODO",
                    commit=self._preserve_work(task, branch),
                )
                return

            log(
                f"END {task_label(task)} status={result.status} attempt={attempt} "
                f"rc={rc}"
            )

            if result.status == "done":
                self._finish_done(task, branch, result)
                return

            if result.status == "blocked":
                self._finalize_on_hold(
                    task,
                    branch=branch,
                    reason=result.reason or "session reported blocked",
                    needed=result.needed or "Review the task and the branch",
                    commit=self._preserve_work(task, branch),
                )
                return

            prior_note = result.reason or "session reported partial"
            log(f"RETRY {task_label(task)} attempt {attempt}: {prior_note}")
            if attempt < MAX_ATTEMPTS:
                time.sleep(RETRY_BACKOFF[min(attempt - 1, len(RETRY_BACKOFF) - 1)])
                continue

            self._finalize_on_hold(
                task,
                branch=branch,
                reason=prior_note,
                needed="Finish the remaining work from the branch and move the task back to TODO",
                commit=self._preserve_work(task, branch),
            )
            return

    def _finish_done(self, task: dict[str, Any], branch: str, result: Any) -> None:
        commit = commit_all(
            self.config.repo,
            f"autopilot: {task_label(task)}",
        )
        merge = merge_to_main(self.config.repo, branch, self.config.base_branch)
        if not merge.merged:
            self._finalize_on_hold(
                task,
                branch=branch,
                reason=f"merge to {self.config.base_branch} failed: {merge.error}",
                needed=f"Resolve branch {branch} and merge it manually",
                commit=commit or "",
            )
            return

        final_commit = merge.commit or commit or ""
        summary = result.summary or "Session reported done without a summary."
        verification = (
            result.verification or "Session reported done without verification details."
        )

        self._write_comment(
            task,
            status="DONE",
            outcome="DONE",
            branch=branch,
            summary=summary,
            verification=verification,
            commit=final_commit,
        )
        log(f"DONE {task_label(task)} commit={final_commit or 'none'}")

    def _finalize_on_hold(
        self,
        task: dict[str, Any],
        *,
        branch: str,
        reason: str,
        needed: str,
        commit: str,
    ) -> None:
        try:
            return_to_main(self.config.repo, self.config.base_branch)
        except GitError as error:
            log(f"WARN could not return to {self.config.base_branch}: {error}")

        self._write_comment(
            task,
            status="ON_HOLD",
            outcome="ON HOLD",
            branch=branch,
            reason=reason,
            needed=needed,
            commit=commit,
        )
        log(f"ON HOLD {task_label(task)}: {reason}")

    def _preserve_work(self, task: dict[str, Any], branch: str) -> str:
        try:
            commit = commit_all(
                self.config.repo,
                f"wip autopilot: {task_label(task)}",
            )
            return commit or ""
        except GitError as error:
            log(f"WARN could not checkpoint branch {branch}: {error}")
            return ""

    def _set_status(self, task: dict[str, Any], status: str) -> None:
        task_id = str(task["id"])
        while True:
            try:
                self.tm.update_task(task_id, status=status)
                return
            except TimeManagerError as error:
                log(f"TM status {status} failed for {task_label(task)}: {error}")
                time.sleep(60)

    def _write_comment(
        self,
        task: dict[str, Any],
        *,
        status: str,
        outcome: str,
        branch: str,
        summary: str = "",
        verification: str = "",
        reason: str = "",
        needed: str = "",
        commit: str = "",
    ) -> None:
        task_id = str(task["id"])
        while True:
            try:
                fresh = self.tm.get_task(task_id)
                description = fresh.get("description") or ""
                comment = build_comment(
                    outcome=outcome,
                    task_title=task_label(task),
                    branch=branch,
                    summary=summary,
                    verification=verification,
                    reason=reason,
                    needed=needed,
                    commit=commit,
                )
                self.tm.update_task(
                    task_id,
                    status=status,
                    description=with_comment(description, comment),
                )
                return
            except TimeManagerError as error:
                log(f"TM update failed for {task_label(task)}: {error}")
                time.sleep(60)


def _safe_name(task: dict[str, Any]) -> str:
    title = str(task.get("title", "task"))
    code = title.split("·", 1)[0].strip().replace(" ", "-")
    safe = "".join(char for char in code if char.isalnum() or char in "-_")
    return safe or str(task.get("id", "task"))[-8:]
