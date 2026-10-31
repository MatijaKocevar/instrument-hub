#!/usr/bin/env bash
# Configure and build Instrument Hub.
#
# Usage:
#   scripts/build.sh [preset]   # preset defaults to "default" (RelWithDebInfo)
#
# Presets live in CMakePresets.json: default, debug, release.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="${1:-default}"

cd "$ROOT"

if [[ ! -f third_party/JUCE/CMakeLists.txt ]]; then
    echo "error: third_party/JUCE is missing; run: git submodule update --init --recursive" >&2
    exit 1
fi

cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel

echo "built: $ROOT/build/engine/instrument-hub"
