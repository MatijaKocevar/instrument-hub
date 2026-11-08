from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path


class GitError(RuntimeError):
    pass


@dataclass(frozen=True)
class MergeResult:
    merged: bool
    commit: str
    error: str = ""


def _git(
    repo: Path, *args: str, check: bool = True
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        message = (
            result.stderr.strip()
            or result.stdout.strip()
            or f"git {' '.join(args)} failed"
        )
        raise GitError(message)
    return result


def ensure_clean_main(repo: Path, base_branch: str) -> None:
    status = _git(repo, "status", "--porcelain").stdout.strip()
    if status:
        raise GitError(
            "working tree is not clean; commit or stash changes before starting autopilot"
        )

    branches = _git(repo, "branch", "--format=%(refname:short)").stdout.splitlines()
    if base_branch not in branches:
        raise GitError(f"base branch {base_branch!r} does not exist")

    _git(repo, "checkout", base_branch)


def branch_exists(repo: Path, branch: str) -> bool:
    result = _git(repo, "rev-parse", "--verify", "--quiet", branch, check=False)
    return result.returncode == 0


def start_branch(repo: Path, branch: str, base_branch: str) -> bool:
    if branch_exists(repo, branch):
        _git(repo, "checkout", branch)
        return True

    _git(repo, "checkout", "-b", branch, base_branch)
    return False


def current_branch(repo: Path) -> str:
    return _git(repo, "branch", "--show-current").stdout.strip()


def short_commit(repo: Path) -> str:
    return _git(repo, "rev-parse", "--short", "HEAD").stdout.strip()


def commit_all(repo: Path, message: str) -> str | None:
    if not _git(repo, "status", "--porcelain").stdout.strip():
        return None

    _git(repo, "add", "-A")
    _git(repo, "commit", "-m", message)
    return short_commit(repo)


def commits_ahead(repo: Path, branch: str, base_branch: str) -> int:
    result = _git(repo, "rev-list", "--count", f"{base_branch}..{branch}")
    return int(result.stdout.strip() or "0")


def merge_to_main(repo: Path, branch: str, base_branch: str) -> MergeResult:
    _git(repo, "checkout", base_branch)

    if commits_ahead(repo, branch, base_branch) == 0:
        _git(repo, "branch", "-D", branch, check=False)
        return MergeResult(merged=True, commit="")

    message = f"Merge autopilot branch {branch}"
    result = _git(repo, "merge", "--no-ff", "-m", message, branch, check=False)
    if result.returncode != 0:
        _git(repo, "merge", "--abort", check=False)
        error = result.stderr.strip() or result.stdout.strip() or "merge failed"
        return MergeResult(merged=False, commit="", error=error)

    commit = short_commit(repo)
    _git(repo, "branch", "-d", branch, check=False)
    return MergeResult(merged=True, commit=commit)


def return_to_main(repo: Path, base_branch: str) -> None:
    _git(repo, "checkout", base_branch)
