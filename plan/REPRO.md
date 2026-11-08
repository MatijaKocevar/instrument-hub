# Reproducing a bug before fixing it

Every bug task starts with a repro that produces the *actual* error text.
No repro, no fix: write `STATUS: blocked` with what was missing.

## UI bugs

The engine serves the API on `127.0.0.1:8080`; the UI is either the
developer's Vite server on `localhost:5173` (proxied to the engine) or the
embedded WebView.

1. Check `curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/state`.
   If it is not `200`, start a bounded engine for the session
   (`build/engine/instrument-hub --serve --no-audio --api-port 8080`) and
   kill it before writing the result. Never kill the developer's instance.
2. Check `curl -s -o /dev/null -w '%{http_code}' http://localhost:5173/`.
   If it is not `200`, start Vite for the session (`cd ui && pnpm dev`) and
   kill it before writing the result. Never leave it running.
3. Open the route in the browser, perform the steps, and collect
   the console output, the failing request (method, URL, status, response
   body), and a screenshot under `/tmp/opencode/`.
4. Describe the element by its visible label and DOM path.

## Engine/API bugs

Reproduce with the running app or a bounded `--serve` instance, then capture
the exact request, response and log lines. Do not add test files.
