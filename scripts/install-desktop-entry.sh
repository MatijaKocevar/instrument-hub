#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APPLICATIONS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ENTRY="$APPLICATIONS/instrument-hub.desktop"

if [[ "${1:-}" == "--remove" ]]; then
    rm -f "$ENTRY"
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$APPLICATIONS" >/dev/null 2>&1 || true
    fi
    echo "removed: $ENTRY"
    exit 0
fi

BINARY="$ROOT/${1:-build}/engine/instrument-hub"

if [[ ! -x "$BINARY" ]]; then
    echo "error: $BINARY is missing; run scripts/build.sh ${1:-} first" >&2
    exit 1
fi

mkdir -p "$APPLICATIONS"

cat > "$ENTRY" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Instrument Hub
GenericName=Instrument Host
Comment=Launchpad-driven multi-instrument engine with a local web UI
Exec="$BINARY"
Path=$ROOT
Icon=$ROOT/engine/assets/instrument-hub.svg
Terminal=false
Categories=AudioVideo;Audio;Midi;
Keywords=audio;midi;synth;launchpad;instrument;
StartupNotify=true
StartupWMClass=Instrument Hub
EOF

chmod +x "$ENTRY"

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$ENTRY"
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPLICATIONS" >/dev/null 2>&1 || true
fi

echo "installed: $ENTRY"
echo "  exec: $BINARY"
echo "  cwd:  $ROOT"
