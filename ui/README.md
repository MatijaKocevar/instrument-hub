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

## Instrument grid

`src/features/grid/` is the main 8×8 grid: families down the side, patches
across, with one level meter per patch and a master meter in the header.

- **Arming** — pressing a patch calls `AppStore.armSlot()`, which updates the
  store optimistically in the same tick and then sends `POST /arm`. The
  highlight therefore moves on the next render (one frame), never after the
  2 s poll. A rejected request rolls the highlight back.
- **Meters** — `MeterStore` subscribes to the `/meters` WebSocket, keeps only
  the newest frame, and reconnects with backoff. `useMeters()` starts it from
  an effect, so server rendering never opens a socket.
- **Loading** — before the first `GET /state`, the grid renders an 8×8
  skeleton (`data-testid="grid-loading"`) instead of flashing an empty grid.
- **Touch** — every patch cell is at least 44 px wide and 68 px high, uses
  `touch-action: manipulation`, and the 8 columns scroll inside the grid on
  narrow screens rather than shrinking below the touch minimum.

The control API currently publishes flat slots, not the set's family tree.
`gridFromState()` maps each slot to a one-patch family so every playable slot
is visible; the view itself is fully family/patch shaped, so the adapter can
be replaced when `/state` carries families.

The grid-specific snapshot (`tests/artifacts/ui-grid-snapshot.html`) renders
8 families × 8 columns with live meter values, and the arm test proves the
highlight changes before the delayed HTTP response arrives.

## Instrument page (`/instrument`)

`src/features/instrument/` is the armed patch's page. It reads the same
`/state` tree as the grid, so the name, family, colour, warm state and chain
latency are the engine's real values.

- **Following** — the page shows the *armed* patch: a Launchpad pad or a grid
  click updates it in place on the next render. A `?slot=` link selects the
  patch only while nothing is armed, so a stale URL cannot override the
  controller. Arming never navigates the user off another page.
- **Mixer** — the volume fader writes `PATCH /patches/{slot} {volumeDb}` (the
  store applies it optimistically and keeps it across polls), balance writes
  `pan`, and the mute/solo buttons write their booleans. The page publishes
  the resolved linear gain it expects the engine to apply.
- **Source** — the declared instrument source with its editor view (descriptor,
  plug-in identity and path; sources are hosted headlessly in this build, so no
  native plug-in window opens yet), and a picker over the `GET /plugins`
  inventory that swaps the live source through `POST /patches/{slot}/source`.
  The engine retires the old instance, warms the new one and re-arms the slot;
  an armed slot is audible again once the new source is resident.
- **Inserts** — the ordered chain from `GET /patches/{slot}/fx` with add,
  remove, move up/down, bypass and a latency budget split into instrument and
  insert totals. Bypass and remove update the chain optimistically and roll
  back on a rejected request.
- **Touch** — every control is at least 44 px (the row buttons use a 40 px
  minimum inside a 44 px row) and the layout reflows at 720 px.

The page is rendered by `tests/artifacts/ui-instrument-snapshot.html`; the
fixture-backed test drives the fader, mute/solo, every chain edit and a source
swap through the real HTTP client.

## Plugins page (`/plugins`)

`src/features/plugins/` is the read-only inventory of everything the VST3
scanner found. It never assigns anything; assignment stays on the patch.

- **Inventory** — each row shows the name, vendor, version, category, format
  and bundle path from `GET /plugins`, with an instrument/effect badge.
- **Search** — filters locally as the user types across name, vendor,
  description, category, id, format and path. Every term must match, so
  `arturia instrument` narrows the list.
- **Rescan** — `POST /plugins/rescan` starts one background scan. The page
  adopts the engine's `scanning: true`, disables the button, and re-reads the
  inventory until the scan settles, so newly installed bundles appear without
  a reload.
- **Used by** — derived from `/state`: a plug-in shows the patch, slot and
  family of every patch whose instrument source is that plug-in. Effects used
  in insert chains are edited on the patch's Instrument page, not here.

The page is rendered by `tests/artifacts/ui-plugins-snapshot.html`; the
fixture-backed test proves search, the used-by hint, and that a rescan picks
up a newly scanned bundle.

## Stage remote (`/remote`)

`src/features/remote/` is the tablet route. Open `http://<engine>:8080/remote`
(or `http://localhost:5173/remote` in development); `main.tsx` picks the route
from the pathname without shipping a router.

- **Touch first** — every button has a 44 px floor; patch rows are 56 px,
  previous/next 56 px+, panic 64 px.
- **Set list** — the engine's slot order, one large target per patch. Tapping
  a row arms it through the same optimistic `AppStore.armSlot()` as the grid.
- **Next / previous** — walks the set order and wraps at both ends. A sparse
  slot list still steps patch-to-patch rather than to a missing slot.
- **Transport** — Play/Pause and Stop (stop rewinds) go through `POST
  /transport`; the buttons update optimistically and roll back on failure.
- **Panic** — stops the transport, rewinds it and disarms every patch.
- **Responsive** — two columns at 820 px and wider, one column below; the set
  list scrolls inside its card and the document never scrolls sideways.

The remote-specific snapshot is `tests/artifacts/ui-remote-snapshot.html`; the
1024×768 browser snapshot is `tests/artifacts/t100-remote-1024x768.png`.

