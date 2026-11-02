# Reproducing a bug before fixing it

Every bug task starts with a repro that produces the *actual* error text.
No repro, no fix: write `STATUS: blocked` with what was missing.

## UI bugs: drive the real app with Playwright

The engine serves the API on `127.0.0.1:8080`; the UI is either the
developer's Vite server on `localhost:5173` (proxied to the engine) or the
embedded WebView. Playwright drives the Vite route, because it is the same
React app.

1. Check `curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/state`.
   If it is not `200`, start a bounded engine for the session
   (`build/engine/instrument-hub --serve --no-audio --api-port 8080`) and
   kill it before writing the result. Never kill the developer's instance.
2. Check `curl -s -o /dev/null -w '%{http_code}' http://localhost:5173/`.
   If it is not `200`, start Vite for the session (`cd ui && pnpm dev`) and
   kill it before writing the result. Never leave it running.
3. With the Playwright tools: set the viewport to 1200x800, navigate to
   `http://localhost:5173/<route>`, perform the steps, then collect
   - the console output (`browser_console_messages`),
   - the failing request (`browser_network_requests`: method, URL, status,
     response body),
   - a screenshot under `tests/artifacts/`.
4. Name the element by its `data-testid`, not by visible text that may
   change.

## Engine/API bugs

Use `curl` against `127.0.0.1:8080` and keep the exact request and response
body. `--serve --no-audio --api-port 0` is fine for a bounded check.

## What goes into `plan/BUGS.md`

Before the fix, the entry must hold:

- steps: numbered, exact route and control
- expected
- actual: the error string the user sees, verbatim
- evidence: console line, request + status + body, screenshot path
- fix: pending, then the commit and what changed

The task's `accept` must say the repro fails before the change and passes
after it. A task that cannot reproduce the reported symptom reports
`STATUS: partial` with the difference, never `done`.
