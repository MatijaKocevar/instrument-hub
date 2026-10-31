#!/usr/bin/env bash
# Configure and build Instrument Hub.
#
# Usage:
#   scripts/build.sh [preset]   # preset defaults to "default" (RelWithDebInfo)
#
# Presets live in CMakePresets.json: default, debug, release.
#
# Every non-Debug build embeds the React UI from ui/dist, so the UI is built
# first. Debug builds load the Vite dev server instead (scripts/dev.sh).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="${1:-default}"

cd "$ROOT"

if [[ ! -f third_party/JUCE/CMakeLists.txt ]]; then
    echo "error: third_party/JUCE is missing; run: git submodule update --init --recursive" >&2
    exit 1
fi

build_ui() {
    local -a PNPM_CMD
    if command -v pnpm >/dev/null 2>&1; then
        PNPM_CMD=(pnpm)
    elif command -v corepack >/dev/null 2>&1; then
        # corepack ships with Node and fetches the version pinned by
        # package.json "packageManager" on first use.
        PNPM_CMD=(corepack pnpm)
    else
        echo "error: pnpm (or corepack) is required to build the React UI" >&2
        exit 1
    fi

    if [[ ! -d ui/node_modules ]]; then
        (cd ui && "${PNPM_CMD[@]}" install --frozen-lockfile)
    fi

    (cd ui && "${PNPM_CMD[@]}" run build)
}

case "$PRESET" in
    debug)
        BUILD_DIR="$ROOT/build-debug"
        ;;
    release)
        BUILD_DIR="$ROOT/build-release"
        build_ui
        ;;
    default|relwithdebinfo)
        BUILD_DIR="$ROOT/build"
        BUILD_PRESET="default"
        build_ui
        ;;
    *)
        echo "error: unknown preset '$PRESET' (expected default, debug or release)" >&2
        exit 1
        ;;
esac

BUILD_PRESET="${BUILD_PRESET:-$PRESET}"

cmake --preset "$BUILD_PRESET"
cmake --build --preset "$BUILD_PRESET" --parallel

echo "built: $BUILD_DIR/engine/instrument-hub"
