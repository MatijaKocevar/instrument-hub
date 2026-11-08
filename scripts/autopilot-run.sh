#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p .autopilot

if [[ " $* " == *" --dry-run "* ]]; then
    exec python3 scripts/autopilot.py "$@"
fi

if [[ -f .autopilot/autopilot.lock ]]; then
    pid="$(cat .autopilot/autopilot.lock 2>/dev/null || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        echo "autopilot already running with pid $pid"
        exit 0
    fi
fi

setsid python3 scripts/autopilot.py "$@" >> .autopilot/autopilot.log 2>&1 < /dev/null &
pid=$!
echo "$pid" > .autopilot/launcher.pid
echo "autopilot started with pid $pid"
echo "log: $ROOT/.autopilot/autopilot.log"
