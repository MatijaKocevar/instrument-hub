#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

WATCH=0
ENGINE_ARGS=()
for argument in "$@"; do
    case "$argument" in
        --watch) WATCH=1 ;;
        *) ENGINE_ARGS+=("$argument") ;;
    esac
done

if [[ ! -x build-debug/engine/instrument-hub ]]; then
    echo "error: build-debug/engine/instrument-hub is missing; build it once with: scripts/build.sh debug" >&2
    exit 1
fi

if ! command -v pnpm >/dev/null 2>&1 && ! command -v corepack >/dev/null 2>&1; then
    echo "error: pnpm (or corepack) is required to run the React UI" >&2
    exit 1
fi

PNPM_CMD=(pnpm)
if ! command -v pnpm >/dev/null 2>&1; then
    PNPM_CMD=(corepack pnpm)
fi

if [[ ! -x ui/node_modules/.bin/vite ]]; then
    echo "installing UI dependencies..."
    (cd ui && "${PNPM_CMD[@]}" install --frozen-lockfile)
fi

(cd ui && exec ./node_modules/.bin/vite) &
VITE_PID=$!

APP_PID=0

cleanup() {
    if [[ "$APP_PID" != "0" ]] && kill -0 "$APP_PID" 2>/dev/null; then
        kill -TERM "$APP_PID" 2>/dev/null || true

        for _ in $(seq 1 30); do
            kill -0 "$APP_PID" 2>/dev/null || break
            sleep 0.1
        done

        if kill -0 "$APP_PID" 2>/dev/null; then
            kill -KILL "$APP_PID" 2>/dev/null || true
        fi

        wait "$APP_PID" 2>/dev/null || true
    fi

    if kill -0 "$VITE_PID" 2>/dev/null; then
        kill "$VITE_PID" 2>/dev/null || true
        wait "$VITE_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM

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

printf '\n  App is ready: http://localhost:5173/\n\n'

export DISPLAY="${DISPLAY:-:0}"

APP_RUNNER=()
if command -v pw-jack >/dev/null 2>&1; then
    APP_RUNNER=(pw-jack)
fi

if [[ "$WATCH" == "0" ]]; then
    "${APP_RUNNER[@]}" ./build-debug/engine/instrument-hub "${ENGINE_ARGS[@]}"
    exit 0
fi

JOBS="${IH_BUILD_JOBS:-$(( $(nproc) / 2 ))}"
[[ "$JOBS" -ge 1 ]] || JOBS=1
BINARY=build-debug/engine/instrument-hub

source_files() {
    find engine/src -type f \
        \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
        -printf '%T@ %p\n' 2>/dev/null | sort
}

start_app() {
    "${APP_RUNNER[@]}" "./$BINARY" "${ENGINE_ARGS[@]}" &
    APP_PID=$!
}

stop_app() {
    if [[ "$APP_PID" == "0" ]]; then
        return
    fi

    if kill -0 "$APP_PID" 2>/dev/null; then
        kill -TERM "$APP_PID" 2>/dev/null || true

        for _ in $(seq 1 50); do
            kill -0 "$APP_PID" 2>/dev/null || break
            sleep 0.1
        done

        if kill -0 "$APP_PID" 2>/dev/null; then
            kill -KILL "$APP_PID" 2>/dev/null || true
        fi

        wait "$APP_PID" 2>/dev/null || true
    fi

    APP_PID=0
}

if [[ -n "$(find engine/src -type f \
        \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
        -newer "$BINARY" -print -quit 2>/dev/null)" ]]; then
    printf '[watch] sources are newer than the binary, building...\n'
    cmake --build build-debug --target instrument-hub --parallel "$JOBS" || exit 1
fi

last="$(source_files)"
start_app
printf '[watch] watching engine/src for C++ changes (Ctrl-C stops)\n'

while true; do
    sleep 1

    if [[ "$APP_PID" != "0" ]] && ! kill -0 "$APP_PID" 2>/dev/null; then
        wait "$APP_PID" 2>/dev/null || true
        APP_PID=0
        printf '[watch] app exited; waiting for a C++ change to relaunch\n'
    fi

    current="$(source_files)"
    if [[ "$current" == "$last" ]]; then
        continue
    fi

    while true; do
        sleep 0.4
        settled="$(source_files)"
        [[ "$settled" == "$current" ]] && break
        current="$settled"
    done
    last="$current"

    printf '\n[watch] C++ change detected, rebuilding...\n'
    stop_app

    if cmake --build build-debug --target instrument-hub --parallel "$JOBS"; then
        printf '[watch] build ok, restarting app\n'
        start_app
    else
        printf '[watch] build failed; fix the errors and save again\n' >&2
    fi
done
