#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import signal
import sys
from pathlib import Path

from autopilot.config import ConfigError, load_config
from autopilot.git_ops import GitError
from autopilot.log import configure, log
from autopilot.runner import AutopilotLock, AutopilotRunner
from autopilot.schedule import DEFAULT_MARGIN_MINUTES
from autopilot.session import stop_active_session
from autopilot.tm import TimeManagerError


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Instrument Hub task autopilot")
    parser.add_argument(
        "--repo", type=Path, default=Path(__file__).resolve().parent.parent
    )
    parser.add_argument(
        "--list",
        default=os.environ.get("IH_TM_LIST", "Instrument Hub"),
        help="Time Manager task list name",
    )
    parser.add_argument("--tm-url", default=None, help="Time Manager MCP URL")
    parser.add_argument(
        "--tm-token", default=None, help="Time Manager MCP bearer token"
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="show the queue and exit"
    )
    parser.add_argument("--once", action="store_true", help="run at most one task")
    parser.add_argument("--max-sessions", type=int, default=0)
    parser.add_argument(
        "--ignore-schedule", action="store_true", help="run outside off-peak hours"
    )
    parser.add_argument(
        "--offpeak-margin-min",
        type=int,
        default=DEFAULT_MARGIN_MINUTES,
        help="do not start within this many minutes of a peak block",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo = args.repo.resolve()

    try:
        config = load_config(
            repo=repo,
            list_name=args.list,
            tm_url=args.tm_url,
            tm_token=args.tm_token,
        )
    except ConfigError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    configure(config.work / "autopilot.log")
    lock = AutopilotLock(config.work / "autopilot.lock")

    if not args.dry_run:
        try:
            lock.acquire()
        except RuntimeError as error:
            log(f"ERROR {error}")
            return 2

    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, stop_active_session)

    runner = AutopilotRunner(config)
    try:
        return runner.run(
            dry_run=args.dry_run,
            max_sessions=max(0, args.max_sessions),
            once=args.once,
            ignore_schedule=args.ignore_schedule,
            margin_minutes=max(0, args.offpeak_margin_min),
        )
    except (GitError, TimeManagerError) as error:
        log(f"ERROR {error}")
        return 2
    finally:
        if not args.dry_run:
            lock.release()


if __name__ == "__main__":
    sys.exit(main())
