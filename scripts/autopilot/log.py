from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path

_LOG_PATH: Path | None = None


def configure(path: Path) -> None:
    global _LOG_PATH
    _LOG_PATH = path
    path.parent.mkdir(parents=True, exist_ok=True)


def log(message: str) -> None:
    line = f"{datetime.now(timezone.utc):%Y-%m-%dT%H:%M:%SZ}  {message}"
    print(line, flush=True)

    if _LOG_PATH is not None:
        _LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with _LOG_PATH.open("a") as handle:
            handle.write(line + "\n")
