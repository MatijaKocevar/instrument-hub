#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

usage() {
    cat <<'EOF'
Usage: scripts/format.sh [options] [base]

  --all              format every C++ source under engine/src (default)
  --changed [base]   format files changed against base (default: main)
  --check            do not write; fail if any file would change
  -h, --help         show this help

The clang-format binary is taken from $CLANG_FORMAT, then PATH, then the
copy bundled with the VS Code C/C++ extension.
EOF
}

MODE="all"
BASE="main"
CHECK=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)
            MODE="all"
            shift
            ;;
        --changed)
            MODE="changed"
            shift
            if [[ $# -gt 0 && "$1" != -* ]]; then
                BASE="$1"
                shift
            fi
            ;;
        --check)
            CHECK=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

resolve_clang_format() {
    if [[ -n "${CLANG_FORMAT:-}" ]]; then
        printf '%s\n' "$CLANG_FORMAT"
        return
    fi

    if command -v clang-format >/dev/null 2>&1; then
        command -v clang-format
        return
    fi

    local bundled
    for bundled in "$HOME"/.vscode/extensions/ms-vscode.cpptools-*/LLVM/bin/clang-format; do
        [[ -f "$bundled" ]] || continue

        if [[ ! -x "$bundled" ]]; then
            local cached="${TMPDIR:-/tmp}/ihub-clang-format"
            if [[ ! -x "$cached" ]] || [[ "$bundled" -nt "$cached" ]]; then
                install -m 755 "$bundled" "$cached"
            fi
            bundled="$cached"
        fi

        printf '%s\n' "$bundled"
        return
    done

    echo "error: clang-format not found" >&2
    echo "install it (sudo apt install clang-format) or set CLANG_FORMAT" >&2
    exit 1
}

collect_files() {
    if [[ "$MODE" == "all" ]]; then
        git ls-files -z -- 'engine/src/*.h' 'engine/src/*.cpp'
        return
    fi

    git diff -z --name-only --diff-filter=ACMR "$BASE" -- 'engine/src/*.h' 'engine/src/*.cpp'
}

CLANG_FORMAT_BIN="$(resolve_clang_format)"

FILES=()
while IFS= read -r -d '' file; do
    FILES+=("$file")
done < <(collect_files)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "nothing to format"
    exit 0
fi

ARGS=(-i)
if [[ $CHECK -eq 1 ]]; then
    ARGS=(--dry-run --Werror)
fi

"$CLANG_FORMAT_BIN" "${ARGS[@]}" --style=file "${FILES[@]}"
