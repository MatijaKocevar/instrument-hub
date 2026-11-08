# Instrument Hub — Handover

> Historical handover from 2026-10-31. The current user-facing state lives in
> [README.md](README.md); the current architecture is
> [docs/architecture.md](docs/architecture.md). In particular, the 0.1.0
> release is Linux-only and the Windows/macOS packaging paths are deferred.

**What this is:** a JUCE 8 / C++20 audio engine with an embedded React UI,
controlled by a Novation **Launchpad Mini MK3** and a local HTTP + WebSocket
API.

**State at handover:** the application is **composed and has routed pages**.
At startup `Main` builds a 64-slot `Session` (fixed `AudioEngine` +
`SlotSwitcher`), loads the default 64-patch set, opens the configured audio
device and arms slot 0. The control API is wired to the live session, and the
Launchpad driver plus `shared/set.yaml` mapping are started when a controller
is present. A pad press reaches audio and the LED mirror.

The React shell now has four workspace pages — **Grid**, **Instrument**,
**Plugins** and **Audio** — plus the existing `/remote` stage route. The Grid
page arms a patch and opens its Instrument page; the Launchpad arms the same
patch and mirrors the grid LEDs (patch colour, armed red, warming yellow,
error flashing). The app and manual checks cover both arm paths, the LEDs and
non-silent audio.

The remaining gaps are `.ihub` open/save, kit/plugin assignment actions, and
physical hardware validation. Section 2 is the blunt list.

## Current phase: bug fixing

The feature roadmap is complete (`T000–T139`) and the app is playable on this
machine: the Launchpad selects patches, the Keystation plays the armed patch,
and per-patch Analog Lab state survives switching and restarts.

**The next phase is bug fixing.** `plan/BUGS.md` is the queue; each entry
carries reproduction steps, expected vs actual and evidence. No new feature or
refactor work starts unless the human asks for it explicitly.

Known current defects:

- `BUG-001`: `GET /audio` reports `open=false` while the session is streaming.
- `BUG-002`: full 64-patch Analog Lab coverage is partly automated (8
  `TestPlugin` patches + 2 real Analog Lab patches; the rest is the manual
  checklist in `docs/manual-analog-lab-64.md`).

Also: launch the app detached. A background `&` from a short-lived shell dies
with `SIGHUP` when the shell exits; that is not an app crash.

| | |
|---|---|
| Repository | `/home/mkocevar/Projects/instrument-hub` |
| Commit at handover | T127 checkpoint (this commit) |
| Date | 2026-10-31 |
| Host verified on | Ubuntu 25.04, Linux 6.14.0-37, x86_64 |
| Test result | `100% tests passed, 0 tests failed out of 58` |
| Product / binary | Instrument Hub / `instrument-hub` |
| Controller | Novation Launchpad Mini MK3 (64 pads, no keys, no encoders) |
| Licence | JUCE AGPLv3 unless a commercial licence is bought (B006) |

Start with [`docs/architecture.md`](docs/architecture.md) for the module map,
dependency direction and the four call paths, then read `plan/DECISIONS.md`
before changing architecture, and `plan/BLOCKERS.md` for the live human queue.

---

## 1. What works

### 1.1 Application shell and UI

- Native 1200×800 JUCE window. Release builds embed the React bundle through
  `juce::WebBrowserComponent` and serve it from a custom resource provider;
  Debug builds load the Vite dev server (`scripts/dev.sh`).
- React + TypeScript + Vite UI with a routed shell (`/grid`, `/instrument`,
  `/plugins`, `/audio`, `/remote`):
  - **Grid page**: 8 families × 8 patches, per-patch colour, armed cell in
    bright red, per-slot and master meters, chain latency, loading skeleton,
    44 px+ touch targets. Clicking a cell arms it and routes to
    `/instrument?slot=N`; per-patch and per-family colours are editable.
  - **Instrument page**: the armed or selected patch's name/family/colour,
    warm state and latency, volume fader, balance, mute/solo, the instrument
    source card with picker/editor, and an insert-chain editor
    (add/remove/move/bypass with instrument + FX latency). It follows the
    controller selection without pulling the user off other pages.
  - **Plugins page**: searchable VST3 inventory with vendor, version,
    category, format, path, instrument/effect badge, "used by" hints, and a
    rescan control that polls until the scan settles. Assignment stays on the
    patch, not here.
  - **Audio page**: the existing audio settings panel (device type/device,
    sample rate, buffer size, channel mapping, latency readout, xrun counter,
    Measure button) plus the MIDI panel (ALSA input switches, Launchpad MI
    status/handshake, Keystation port, 64-pad device test) and the LAN pairing
    card (real QR code, 6-digit code, token storage).
  - `/remote` stage route: set list, previous/next patch, play/pause/stop,
    panic, reconnect banner, wake lock, high-contrast theme.
  - Polling state store with exponential-backoff reconnect and optimistic arm.

### 1.2 Control API

Runs on `127.0.0.1:8080` by default; `--serve` runs it headless.

- `GET /state`, `POST /arm`, `GET/POST /settings`, `GET/POST /transport`
- `PATCH /patches/{slot}` (mix, mute/solo, colour),
  `POST /patches/{slot}/source`, and `GET/POST/PUT/PATCH/DELETE
  /patches/{slot}/fx` (insert chain)
- `GET /audio`, `POST /audio`, `POST /audio/measure`
- `GET /plugins`, `POST /plugins/rescan`
- `GET /midi`, `POST /midi/inputs`, `POST /midi/pad-test`
- `GET /meters` (WebSocket, ~30 Hz), `GET /events` (WebSocket state mirror)
- `GET /pairing`, `POST /pairing/lan`, `POST /pairing/revoke`, `POST /pair`
- Token auth (`X-Instrument-Hub-Token` or bearer) and loopback-only management
  endpoints; LAN mode is off by default and rebinds to all interfaces when on.
- mDNS advertisement of `_instrumenthub._tcp` while LAN mode is enabled
  (POSIX; Windows returns "not implemented" — see section 2).
- The API is attached to the composed session by `Api::attachSession()`:
  `POST /arm` publishes into `SlotSwitcher`, `POST /transport` drives the
  session transport, `GET /state` reports live slot state/gain, and `/meters`
  reads the real slot and master-bus peaks captured on the audio thread.

### 1.3 Audio path and DSP

- `Session`: 64 fixed logical slots, all wired in parallel into a summing bus
  at startup. The warm pool keeps the armed slot plus its neighbours
  instantiated; arming changes which slots process and applies a 3 ms
  equal-power gain ramp. Nothing is rewired at runtime.
- `AudioEngine`: master limiter, no allocation/locks/logging in
  `processBlock()`.
- `Slot` / `FxChain` / `SlotSwitcher` / `WarmPool`: instruments → ordered FX →
  bus, 3 ms crossfades, warm-slot preload/LRU eviction.
- `RtGuard`: allocation/lock/log detector.
- `TimingMonitor`: callback deadline tracking, xruns and rolling maximum.
- `LatencyProbe`: ALSA loopback impulse/cross-correlation measurement exposed
  by `--measure-latency` and `POST /audio/measure`.
- `Transport`, `ClockOut`, `ClockIn`, `TapTempo`: sample-accurate BPM/PPQ,
  24 PPQN MIDI clock in/out, tap tempo.
- Built-in FX: 3-band EQ, feed-forward compressor, tempo-syncable delay,
  Freeverb-style reverb.
- `AudioInputInstrument`: first-class input slot with direct monitoring that
  bypasses the FX chain.

### 1.4 Instruments, plugins and sampler

- `Instrument` interface with plugin, audio-input and sampler implementations.
- VST3 support: out-of-process scanner (`ihub-scanner`), cache/dead-man's
  pedal list, `PluginInstrument` state round-trip, embedded editor window,
  `PluginFx` as an FX-chain element.
- Optional out-of-process sandbox (`ihub-sandbox` + `SandboxHost`): a crashing
  or hanging plugin kills only the helper; the slot is marked failed and the
  rest of the engine keeps rendering.
- Sampler: WAV/AIFF/FLAC loader with resampling and RT-safe handoff; JSON kit
  format with velocity layers/round-robin; 64-voice polyphony, one-shot/gate
  modes and choke groups.
- An in-repo `TestPlugin.vst3` covers all plugin tests without third-party
  binaries.

### 1.5 Session composition (T114/T115)

- `Session` owns the `AudioEngine`, one `SlotSwitcher` over its slots, the
  loaded instruments, the `DeviceManager` and the session `Transport`.
- `Session::build()` creates the graph from `SessionSlotConfig` entries
  (init tone, sampler, plugin or audio input) and prepares every buffer once.
- `Main::startSession()` builds the default 64-slot, 8-family set, opens the
  device (`--audio-type`, `--audio-device`, `--audio-sample-rate`,
  `--audio-buffer-size`) and arms slot 0. The app makes sound on a fresh
  checkout with no samples or plugins. `shared/set.yaml` supplies the 64
  family/patch descriptors, colours, mix and insert chains; the `.ihub`
  container is still loaded by tests and the project layer, not by the app.
- `Session::measureSwitch()` renders the armed slot, switches, and compares the
  result with the ideal 3 ms equal-power crossfade.
- `Session::render()` is the offline/acceptance path and the device callback
  `audioDeviceIOCallbackWithContext()` is the real-time path; both drive the
  same fixed graph.

### 1.6 Controller (Launchpad Mini MK3)

- Pure `LaunchpadProtocol`: Novation SysEx header
  `F0 00 20 29 02 0D`, programmer mode, external LED feedback, wake, 8×8 grid
  notes 11..88, top row CC 91..98, right column CC 89..19, palette and LED
  modes.
- `LaunchpadDriver`: detects the `Launchpad Mini MK3 MI` port in both
  directions, sends the programmer-mode handshake on connect, decodes pads and
  control buttons, writes idle/warming/active/error LEDs, and leaves programmer
  mode on disconnect. A hardware-free `injectMidiMessage()` / `setMessageSink()`
  seam feeds the same decoder and captures the same encoder as the MIDI thread.
- `Mapping`: `shared/set.yaml` loader with 8 families × 8 patches, macro banks,
  scenes, validation and hot reload. A pad press resolves to a slot and arms it.
- `Main::startController()` starts the driver and the mapping; pads arm slots,
  and the API/UI mirror the selection. Top-row and right-column selections
  light their buttons.
- LED mirror: the armed pad is bright red, a warming pad yellow, a load error
  flashes red, and every other pad shows its patch's palette colour. The
  100 ms poll repaints after an HTTP colour edit, so the hardware follows the
  Grid page.
- Tests: protocol tables, all 64 pad bindings, scripted driver event/LED
  stream, hot reload, the pad → session end-to-end path, and the T127
  click/pad/LED acceptance.

Full reference: `docs/launchpad-mini-mk3.md`.

### 1.7 Project, diagnostics and packaging

- `.ihub` set container (ZIP: `set.json`, `mapping.yaml`,
  `plugin-state/<uuid>.bin`), atomic save, bounded all-or-nothing load,
  schema migration (v1 → v2), missing-plugin placeholders with warnings.
- Structured log ring, crash-safe dump on fatal signals, and
  `--diagnostics` report (device, buffer, latency, xruns, plugins, log tail).
- CMake presets `default` (RelWithDebInfo), `debug`, `release`; Ninja; the
  React UI is built automatically by CMake.
- GitHub Actions: three-platform compile matrix (`build.yml`) and unsigned
  packaging (`release.yml`: AppImage / MSI / DMG). Only the Linux AppImage has
  been produced and executed locally; see B005.

---

## 2. What is stubbed / not yet wired

The app now composes and plays a session. These are the remaining gaps, in the
order that matters for a usable instrument.

1. **The set/project layer has no open/save UI or API.** There is no Open/Save
   `.ihub` action, no set browser, and no `GET/POST /set` endpoint. The
   serializer and migration are exercised by tests only, so Grid edits live in
   memory for the life of the process.
2. **The default source is still an init tone.** `shared/set.yaml` supplies the
   64 family/patch descriptors, colours, mix and insert chains, but the app
   does not instantiate a kit or VST3 from those descriptors. Sampler, plugin
   and audio-input slots are implemented in `Session::build()` and reachable
   through `POST /patches/{slot}/source`, but choosing one is not yet a UI
   action.
3. **Sampler and plugin browsers have no assignment UI.** The Plugins page is a
   read-only inventory with search/rescan (as specified); it deliberately has
   no assignment control. Kit selection has no page. Assignment stays a
   code/API path.
4. **The Grid is the real 8×8 tree.** `/state` now publishes families →
   patches with names, colours, warm state and latency, and the Grid page
   renders 64 cells. The remaining persistence gap is item 1: a colour, mix or
   source edit is not written back to a `.ihub` container yet.
5. **Page interaction needs a real browser.** No browser/tablet has driven the
   pages yet; B007/B009 cover the physical checks.
6. **mDNS is POSIX-only.** `Discovery::start()` returns
   "mDNS discovery is not implemented on this platform" on Windows. Linux has
   been verified against Avahi; macOS is untested.
7. **Missing plugins become silent placeholders by design.** This is intended
   (T081) and reported as a warning, not a crash, but a set referencing an
   absent plugin will not make sound for that slot.
8. **Third-party plugins are not validated.** `TestPlugin` covers the host
   contract. Analog Lab Pro and EZdrummer 3 need a licensed Windows/macOS
   machine (B003).
9. **Packaging is unsigned.** The MSI, DMG and AppImage have no signature,
   notarisation or timestamp. Windows/macOS packages are compile-only until
   B005 is done.
10. **No physical controller or interface run.** All latency/xrun figures come
    from the ALSA Loopback; all controller tests use the driver's injection
    seam. See section 4 and B004/B009.

### The shortest path to a saved, editable set

1. Expose `.ihub` open/save through the API (`GET/POST /set`) and add the UI
   actions; persist the Grid/Instrument edits that already flow through the
   API.
2. Let the set descriptors instantiate their real source (kit or VST3) instead
   of an init tone, using the existing `POST /patches/{slot}/source` path.
3. Add kit and plugin selection per patch (the factories and the plugin
   inventory already exist).
4. Run the hardware checks in section 4.

---

## 3. Build and run

### 3.1 Common prerequisites

- Git checkout with the JUCE submodule:
  `git submodule update --init --recursive`
- CMake ≥ 3.22 and Ninja.
- Node.js 20+ and pnpm (the repo pins pnpm 11.24.0 via `packageManager`;
  `corepack pnpm` works without a global install).
- On any platform the first non-Debug build also builds the React UI into
  `ui/dist` and embeds it.
- The controller tests and the ALSA Loopback acceptance are Linux-only; the
  rest of the suite builds on all three platforms.

The build produces `instrument-hub`, `ihub-scanner` and `ihub-sandbox` in the
CMake binary directory (`build/engine/`, `build-release/engine/`, etc.).

### 3.2 Linux

One-time host setup (this is B001, resolved):

```bash
sudo apt install build-essential pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype6-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev libcurl4-openssl-dev \
  ninja-build cmake
```

`scripts/bootstrap.sh --install` performs that install, loads `snd-aloop` /
`snd-virmidi`, checks the toolchain and then builds the app. If the machine
is already set up, `scripts/bootstrap.sh --check` is read-only.

Build:

```bash
scripts/build.sh                 # RelWithDebInfo into build/
```

Debug UI development (Vite dev server + Debug binary):

```bash
scripts/dev.sh                   # builds Debug and runs Vite on :5173
```

Run:

```bash
./build/engine/instrument-hub              # window + audio + control API + controller
./build/engine/instrument-hub --serve      # headless API
./build/engine/instrument-hub --no-audio   # UI/API without opening a device
```

Useful one-shot modes:

```bash
./build/engine/instrument-hub --list-devices
./build/engine/instrument-hub --measure-latency
./build/engine/instrument-hub --diagnostics
```

`--api-port`, `--api-host`, `--api-token` and `--lan` change the control
server. `--audio-type`, `--audio-device`, `--audio-sample-rate` and
`--audio-buffer-size` choose the session device. The mapping is found through
`$INSTRUMENT_HUB_SET`, then `<cwd>/set.yaml`, then `<cwd>/shared/set.yaml`;
run from the repository root for the shipped mapping. JACK warnings on a
machine without JACK are harmless; ALSA still enumerates.

### 3.3 Windows

Prerequisites:

- Visual Studio 2022 Build Tools with the C++ workload and Windows 10/11 SDK.
- CMake, Ninja, Git, Node.js 20+ and pnpm.
- WebView2 SDK. `engine/CMakeLists.txt` sets `NEEDS_WEBVIEW2`, and JUCE does
  not download the SDK, so install the NuGet package first (the `build.yml`
  workflow uses `Microsoft.Web.WebView2` 1.0.3485.44). The WebView2 Runtime is
  also required on the target machine.

From a **Developer Command Prompt for VS 2022** (so `cl.exe` is on `PATH`):

```bat
git submodule update --init --recursive
cd ui
pnpm install --frozen-lockfile
pnpm run build
cd ..
cmake --preset default
cmake --build --preset default --parallel
build\engine\instrument-hub.exe
```

Run modes are the same flags as Linux. The app builds and runs without extra
acceptance tooling on Windows.

### 3.4 macOS

Prerequisites:

- Xcode Command Line Tools (`xcode-select --install`).
- CMake, Ninja, Git, Node.js 20+ and pnpm.

```bash
git submodule update --init --recursive
cd ui
pnpm install --frozen-lockfile
pnpm run build
cd ..
cmake --preset default
cmake --build --preset default --parallel
open build/engine/*.app          # JUCE produces an app bundle
```

If Gatekeeper blocks the unsigned bundle, right-click → Open, or
`xattr -dr com.apple.quarantine build/engine/*.app`.

The app uses the system WKWebView; no extra runtime is needed. Run modes are
the same as Linux. The app is unsigned and unnotarised.

### 3.5 Packaging

`.github/workflows/release.yml` builds and uploads:

- Linux x86_64 AppImage (`instrument-hub-linux-appimage`)
- Windows x64 MSI (`instrument-hub-windows-msi`)
- macOS DMG (`instrument-hub-macos-dmg`)

Trigger it by pushing a `v*` tag or with `workflow_dispatch`. All packages are
unsigned. The Linux job builds the AppImage, then executes it with
`--list-devices` under Xvfb.

---

## 4. Hardware checks still outstanding

Every item below needs a human and physical hardware. The software-side
workarounds are noted so a failure can be triaged.

| # | Hardware / environment | Check | Workaround until done |
|---|---|---|---|
| B009 (supersedes B002) | Novation Launchpad Mini MK3 | Connect the controller. Confirm (a) the programmer-mode SysEx is accepted and the surface leaves Live mode, (b) pads report notes 11..88 on the `MI` port and release with velocity 0, (c) idle/warming/active/error LEDs are the expected green/yellow/red, (d) top-row CC 91..98 selects banks and right-column CC 89..19 selects scenes, (e) pads 0..7 arm audible slots. Record any byte mismatch; tables live in `LaunchpadProtocol.cpp`. | `ihub-launchpad-protocol/mapping/driver` scripted tests + `ihub-e2e` on the Loopback |
| B003 | Windows music PC with Analog Lab Pro and EZdrummer 3 | Load each in Instrument Hub, confirm the editor opens, presets load, state saves and reloads, and record reported latency. | In-repo `TestPlugin.vst3`; an Analog Lab V cache entry may exist but has not been validated |
| B004 | Real audio interface | Connect the interface, try 64/128/256 frames at 48 kHz, record xrun counts and measured round-trip, and update the default in `DECISIONS.md` if it differs. An interface may enumerate (the current host lists a Steinberg UR22mkII) but no measurements exist. | ALSA Loopback figures are a lower bound, not a promise |
| B005 | Windows and macOS machines | Run the CI artifacts on each OS; confirm the window opens, an audio device enumerates and plugin scanning works. | CI compile success only |
| B007 | Tablet | Scan the QR from the tablet, confirm pairing, set list, tempo control, and that Launchpad pad presses highlight on the tablet. | Responsive browser snapshots at tablet viewports |

Non-hardware open items from `BLOCKERS.md`:

- **B006 · JUCE licence decision** — decide AGPLv3 (publish the engine source)
  or buy a commercial JUCE licence before distributing a closed-source binary.
  Private use is unaffected.
- **B008 · Chinese public holidays** — the autopilot's off-peak scheduler treats
  Chinese public holidays as normal weekdays; add dates to `HOLIDAYS` in
  `autopilot.py` if a run lands on one. Does not affect the product.

---

## 5. Open items copied from `plan/BLOCKERS.md`

`B001` is **resolved** (C++ headers installed). The following are **open** at
handover and must be closed by a human:

- **B009 · Launchpad Mini MK3 physical verification** (supersedes B002) — the
  protocol, driver and mapping are tested against scripted MIDI; real firmware
  may disagree on programmer-mode entry, LED colours or port behaviour.
  `status: open`, `severity: soft`. Human: connect the Launchpad and run the
  checklist in section 4.
- **B003 · Analog Lab Pro and EZdrummer 3 validation** — third-party plugins are
  not installed/licensed on this machine. `status: open`, `severity: soft`.
  Human: on the Windows music PC, confirm editor, presets, state save/reload and
  reported latency for each.
- **B004 · Real audio interface tuning** — all latency and xrun figures come
  from the ALSA loopback. `status: open`, `severity: soft`. Human: connect the
  real interface, try 64/128/256 at 48 kHz, record xruns and round-trip, and
  update `DECISIONS.md` if the default differs.
- **B005 · Windows and macOS runtime verification** — CI compiles all three
  platforms but nothing executes on Windows or macOS.
  `status: open`, `severity: soft`. Human: run the CI artifacts on each OS;
  confirm window, audio enumeration and plugin scan.
- **B006 · JUCE licence decision** — JUCE 8 is AGPLv3 or commercial.
  `status: open`, `severity: soft`. Human: decide open-source or buy a licence;
  private use is unaffected.
- **B007 · Tablet remote physical test** — the remote is tested in a desktop
  browser at tablet viewport sizes. `status: open`, `severity: soft`. Human:
  scan the QR from the tablet, confirm pairing, set list, tempo control and
  Launchpad pad-press highlighting.
- **B008 · Chinese public holidays** — off-peak scheduling treats Chinese public
  holidays as normal weekdays. `status: open`, `severity: soft`. Human: add
  dates to `HOLIDAYS` in `autopilot.py` if a run lands on one.

---

## 6. Build check

Build the affected target and launch the app. Testing is disabled; do not
create or run tests.

---

## 7. Repo map

```
engine/src/
  Main.cpp              application shell, CLI modes, web view, API, session,
                        Launchpad + mapping startup
  Session.{h,cpp}       composed 64-slot session, warm pool, device ownership,
                        switch measurement, real meters
  api/                  HTTP + WebSocket server, state store, session bridge,
                        pairing, mDNS
  audio/                engine, slots, bus, device manager, transport, clocks,
                        latency probe, timing monitor, RT guard
  controller/           Launchpad protocol/driver and set.yaml mapping
  dsp/                  EQ, compressor, delay, reverb
  instruments/          Instrument interface, audio input, plugin FX
  plugins/              scanner, cache, plugin instance/window, sandbox
  project/              .ihub set format, serializer and migrations
  sampler/              sample loader, kit format, polyphonic sampler
  diag/                 log ring, crash dump, diagnostics report
  Bridge/               web view and embedded-asset provider
shared/                 openapi.yaml, schemas, set.yaml
ui/                     React app (Grid, Instrument, Plugins, Audio, /remote)
scripts/                build.sh, dev.sh, bootstrap.sh
plan/                   roadmap, decisions, blockers, autopilot
docs/                   Architecture map, Launchpad reference, new-machine setup
```

Start with `plan/AUTOPILOT.md` for the session protocol, then section 2 above
for the remaining integration work, then the relevant module.
