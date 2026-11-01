#!/bin/sh

set -eu

bin=${1:?usage: serve-smoke.sh <instrument-hub>}
log=$(mktemp "${TMPDIR:-/tmp}/ihub-serve.XXXXXX")
pid=

cleanup()
{
    if [ -n "$pid" ]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi

    rm -f "$log"
}

trap cleanup EXIT INT TERM

"$bin" --serve --no-audio --api-port 0 >"$log" 2>&1 &
pid=$!

i=0

while [ "$i" -lt 100 ]; do
    if grep -q 'serve: control API is running' "$log" 2>/dev/null; then
        break
    fi

    if ! kill -0 "$pid" 2>/dev/null; then
        echo "serve-smoke: process exited before reporting readiness" >&2
        cat "$log" >&2
        exit 1
    fi

    i=$((i + 1))
    sleep 0.1
done

if ! grep -q 'serve: control API is running' "$log"; then
    echo "serve-smoke: no readiness line within 10 s" >&2
    cat "$log" >&2
    exit 1
fi

port=$(sed -n 's/.*api: listening on http:\/\/127\.0\.0\.1:\([0-9][0-9]*\).*/\1/p' "$log" | head -n 1)

if [ -z "$port" ]; then
    echo "serve-smoke: no bound API port in the output" >&2
    cat "$log" >&2
    exit 1
fi

if command -v curl >/dev/null 2>&1; then
    curl -sf "http://127.0.0.1:$port/state" >/dev/null
fi

echo "serve-smoke: control API answered on port $port"
exit 0
