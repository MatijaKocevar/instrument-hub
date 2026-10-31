#!/usr/bin/env bash
# Run the Vite dev server and the Debug engine together.
#
# Usage:
#   scripts/dev.sh [engine arguments...]
#
# The Debug build of WebView opens http://localhost:5173, so saving any file in
# ui/src triggers Vite's HMR update inside the embedded web view. Stop with
# Ctrl-C; the dev server is cleaned up on the way out.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v npm >/dev/null 2>&1; then
    echo "error: npm is required to run the React UI" >&2
    exit 1
fi

if [[ ! -f third_party/JUCE/CMakeLists.txt ]]; then
    echo "error: third_party/JUCE is missing; run: git submodule update --init --recursive" >&2
    exit 1
fi

if [[ ! -x ui/node_modules/.bin/vite ]]; then
    echo "installing UI dependencies..."
    (cd ui && npm ci --no-audit --no-fund)
fi

# Configure/build first, then start Vite: the app reconnects to HMR as soon as
# the window appears, and a compile error does not leave a stray server behind.
cmake --preset debug
cmake --build --preset debug --parallel

(cd ui && exec ./node_modules/.bin/vite) &
VITE_PID=$!

cleanup() {
    if kill -0 "$VITE_PID" 2>/dev/null; then
        kill "$VITE_PID" 2>/dev/null || true
        wait "$VITE_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

for _ in $(seq 1 300); do
    if ! kill -0 "$VITE_PID" 2>/dev/null; then
        echo "error: Vite exited before the dev server was ready" >&2
        exit 1
    fi

    if command -v curl >/dev/null 2>&1 && curl -fsS http://localhost:5173/ >/dev/null 2>&1; then
        break
    fi

    if ! command -v curl >/dev/null 2>&1; then
        sleep 1
        break
    fi

    sleep 0.1
done

if command -v curl >/dev/null 2>&1 && ! curl -fsS http://localhost:5173/ >/dev/null 2>&1; then
    echo "error: Vite did not become ready on http://localhost:5173" >&2
    exit 1
fi

export DISPLAY="${DISPLAY:-:0}"
./build-debug/engine/instrument-hub "$@"
