#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-$(sed -n 's/^project(InstrumentHub VERSION \([^ ]*\).*/\1/p' "$ROOT/CMakeLists.txt")}"
OUT_DIR="${2:-$ROOT/dist}"
BUILD_DIR="${IHUB_BUILD_DIR:-$ROOT/build-release/engine}"
ICON="$ROOT/engine/assets/instrument-hub.svg"
APPIMAGETOOL="${APPIMAGETOOL:-}"
APPIMAGETOOL_URL="${APPIMAGETOOL_URL:-https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage}"

for target in instrument-hub ihub-scanner ihub-sandbox; do
    if [[ ! -x "$BUILD_DIR/$target" ]]; then
        echo "error: $BUILD_DIR/$target is missing; build the release targets first" >&2
        exit 1
    fi
done

if [[ ! -f "$ICON" ]]; then
    echo "error: icon is missing: $ICON" >&2
    exit 1
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/ihub-appimage.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

APPDIR="$WORK/InstrumentHub.AppDir"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/scalable/apps"

install -m755 "$BUILD_DIR/instrument-hub" "$APPDIR/usr/bin/instrument-hub"
install -m755 "$BUILD_DIR/ihub-scanner"     "$APPDIR/usr/bin/ihub-scanner"
install -m755 "$BUILD_DIR/ihub-sandbox"     "$APPDIR/usr/bin/ihub-sandbox"
install -m644 "$ICON"                       "$APPDIR/instrument-hub.svg"

for lib in libstdc++.so.6 libgcc_s.so.1; do
    libpath="$(ldd "$BUILD_DIR/instrument-hub" | awk -v name="$lib" '$1 == name { print $3 }')"
    if [[ -n "$libpath" && -f "$libpath" ]]; then
        cp -L "$libpath" "$APPDIR/usr/lib/"
    fi
done

cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
set -eu
HERE="$(dirname "$(readlink -f "$0")")"
APPDIR="${APPDIR:-$HERE}"
if [ -d "$APPDIR/usr/lib" ]; then
    LD_LIBRARY_PATH="$APPDIR/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi
exec "$APPDIR/usr/bin/instrument-hub" "$@"
APPRUN
chmod 755 "$APPDIR/AppRun"

cat > "$APPDIR/instrument-hub.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Instrument Hub
GenericName=Audio Workbench
Comment=Eight-track performance hub for VST3 instruments and the Launchpad Mini MK3
Exec=instrument-hub %U
Icon=instrument-hub
Terminal=false
Categories=AudioVideo;Audio;Music;
StartupWMClass=Instrument Hub
X-AppImage-Version=$VERSION
DESKTOP

cp "$APPDIR/instrument-hub.desktop" "$APPDIR/usr/share/applications/"
cp "$APPDIR/instrument-hub.svg" \
   "$APPDIR/usr/share/icons/hicolor/scalable/apps/instrument-hub.svg"

if [[ -z "$APPIMAGETOOL" ]]; then
    APPIMAGETOOL="$WORK/appimagetool"
    curl --fail --location --retry 3 --output "$APPIMAGETOOL" "$APPIMAGETOOL_URL"
    chmod 755 "$APPIMAGETOOL"
fi

mkdir -p "$OUT_DIR"
OUT="$OUT_DIR/Instrument-Hub-${VERSION}-x86_64.AppImage"
rm -f "$OUT"
ARCH=x86_64 "$APPIMAGETOOL" \
    --appimage-extract-and-run \
    --no-appstream \
    "$APPDIR" "$OUT"
chmod 755 "$OUT"
echo "$OUT"
