# Instrument Hub UI

React + TypeScript + Vite. The same app serves the embedded desktop view and
the tablet remote (different routes, added by later tasks).

## Commands

```bash
corepack pnpm install        # pnpm is pinned by "packageManager"
corepack pnpm run dev        # Vite on :5173, proxies the API to :8080
corepack pnpm run build      # regenerate the client, typecheck, build dist/
corepack pnpm run test:snapshot
```

`pnpm` may be installed directly; `corepack pnpm ...` works anywhere Node is.

## Typed client

`src/api/schema.d.ts` is generated from `../shared/api/openapi.yaml` by
`openapi-typescript`. `pnpm build` regenerates it before typechecking, so the
UI cannot drift from the API contract. Never edit it by hand; change the
OpenAPI document and rebuild.

`src/api/client.ts` wraps the generated types in an `openapi-fetch` client and
adds a runtime shape check, an `ApiError`, and optional token auth via
`X-Instrument-Hub-Token`.

## State and reconnect

`src/state/store.ts` holds the last good `GET /state` snapshot. It polls every
2 s, keeps the stale snapshot visible while the engine restarts, and retries
with exponential backoff (250 ms doubling to 5 s). Requests are serialised, so
a slow engine cannot stack them. `main.tsx` also refreshes on `online` and
window `focus`.

Components read the store through `useAppState()` (`useSyncExternalStore`),
which works both in the browser and under `react-dom/server`.

## Headless snapshot test

`pnpm run test:snapshot` builds `src/testing/snapshot.tsx` for Node and runs
it. React renders the real app with `react-dom/server` — no browser and no
Chrome — against a fixture HTTP server that speaks the control API. The test
asserts the widget count and content, writes the markup to
`tests/artifacts/ui-state-snapshot.html`, and separately proves the store
reconnects after HTTP failures and backs off on repeated failures.

To render against a running engine instead of the fixture:

```bash
./build/engine/instrument-hub --serve --api-port 18080 &
node dist-test/snapshot.mjs --api=http://127.0.0.1:18080
```

The live snapshot lands in `tests/artifacts/ui-state-live.html`.
