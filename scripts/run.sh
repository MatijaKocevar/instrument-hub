#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BINARY="${IHUB_BINARY:-build-release/engine/instrument-hub}"

if [[ ! -x "$BINARY" ]]; then
    echo "error: $BINARY is missing; build it once with: scripts/build.sh release" >&2
    exit 1
fi

exec "$ROOT/$BINARY" "$@"
