#!/usr/bin/env bash
# Put a fresh Linux box into the same state as the machine the autopilot has
# been running on: toolchain, headers, audio devices, submodule, UI deps,
# git identity, opencode credentials, build, tests, then the driver.
#
# What it cannot do for you is `sudo`. It checks those items, prints the exact
# commands, and stops. Run them, then run this script again.
#
# Usage:
#   scripts/bootstrap.sh              # check everything, build, test
#   scripts/bootstrap.sh --check      # diagnostics only, change nothing
#   scripts/bootstrap.sh --no-build   # checks + deps, skip build/test
#   scripts/bootstrap.sh --start      # checks + build, then run the autopilot

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD=1
START=0
for arg in "$@"; do
    case "$arg" in
        --check)    BUILD=0 ;;
        --no-build) BUILD=0 ;;
        --start)    START=1 ;;
        -h|--help)  sed -n '2,14p' "$0"; exit 0 ;;
        *) echo "unknown flag: $arg" >&2; exit 2 ;;
    esac
done

MODEL="${IH_MODEL:-deepseek/deepseek-flash}"
fail=0
ok()   { printf '  ok    %s\n' "$*"; }
miss() { printf '  MISS  %s\n' "$*"; fail=1; }
warn() { printf '  warn  %s\n' "$*"; }

echo "repo: $ROOT"

# ---------------------------------------------------------------- toolchain
echo
echo "toolchain"
for cmd in git cmake ninja pkg-config node npm python3 opencode; do
    if command -v "$cmd" >/dev/null 2>&1; then
        ok "$cmd $("$cmd" --version 2>/dev/null | head -1)"
    else
        miss "$cmd is not on PATH"
    fi
done

echo
echo "headers"
for mod in alsa jack freetype2 x11 gl webkit2gtk-4.1; do
    if pkg-config --exists "$mod" 2>/dev/null; then
        ok "pkg-config $mod"
    else
        miss "pkg-config $mod"
    fi
done

# ---------------------------------------------------------------- audio/midi
echo
echo "audio and midi"
if [[ -e /dev/snd/seq ]]; then
    ok "ALSA sequencer /dev/snd/seq"
else
    miss "ALSA sequencer missing (needed by the virtual Launchkey)"
fi
if aplay -l 2>/dev/null | grep -qi loopback; then
    ok "ALSA loopback card"
else
    miss "ALSA loopback card (needed by the latency/xrun tests)"
fi
rtprio="$(ulimit -r 2>/dev/null || echo 0)"
if [[ "$rtprio" =~ ^[0-9]+$ ]] && (( rtprio > 0 )); then
    ok "realtime priority (rtprio $rtprio)"
else
    warn "no realtime priority: audio tests run without SCHED_FIFO"
fi
if id -nG | tr ' ' '\n' | grep -qx audio; then
    ok "member of the audio group"
else
    warn "not in the audio group (needs the group added, then a re-login)"
fi

# ---------------------------------------------------------------- repo
echo
echo "repo"
if [[ -f third_party/JUCE/CMakeLists.txt ]]; then
    ok "JUCE submodule present"
else
    warn "JUCE submodule missing, fetching"
    git submodule update --init --recursive || miss "git submodule update failed"
fi

name="$(git config user.name 2>/dev/null || true)"
email="$(git config user.email 2>/dev/null || true)"
if [[ -z "$email" ]]; then
    git config user.name  "Instrument Hub Autopilot"
    git config user.email "autopilot@instrument-hub.local"
    ok "git identity set (repo-local)"
else
    ok "git identity: $name <$email>"
fi

# ---------------------------------------------------------------- opencode
echo
echo "opencode"
if command -v opencode >/dev/null 2>&1; then
    if opencode run --help 2>/dev/null | grep -q -- '--auto'; then
        ok "run --auto supported"
    else
        miss "opencode run --auto is missing (upgrade opencode)"
    fi
    if opencode auth list 2>/dev/null | grep -qi 'deepseek'; then
        ok "DeepSeek credentials stored"
    else
        miss "no DeepSeek credentials (opencode auth login)"
    fi
    if opencode models 2>/dev/null | grep -qx "$MODEL"; then
        ok "model $MODEL"
    else
        miss "model $MODEL not available (export IH_MODEL=<provider/model>)"
    fi
fi

# ---------------------------------------------------------------- sudo queue
if (( fail )); then
    cat <<'EOF'

The items above need a human. On Ubuntu:

  sudo apt install build-essential pkg-config libasound2-dev libjack-jackd2-dev \
    libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev \
    libglu1-mesa-dev libcurl4-openssl-dev ninja-build cmake nodejs npm

  sudo modprobe snd-aloop snd-virmidi      # loopback audio + virtual MIDI

Re-run this script afterwards.
EOF
    exit 1
fi

# ---------------------------------------------------------------- build
if (( BUILD )); then
    if [[ ! -d ui/node_modules ]]; then
        echo
        echo "ui dependencies"
        npm --prefix ui ci || { echo "npm ci failed" >&2; exit 1; }
    fi

    echo
    echo "build"
    scripts/build.sh || { echo "build failed" >&2; exit 1; }

    echo
    echo "tests"
    ctest --test-dir build --output-on-failure || { echo "tests failed" >&2; exit 1; }
fi

echo
echo "ready."
echo "  python3 plan/autopilot.py --dry-run    # confirm the queue"
echo "  python3 plan/autopilot.py              # run (docs/new-machine.md §9 to stop)"

if (( START )); then
    echo
    exec python3 "$ROOT/plan/autopilot.py"
fi
