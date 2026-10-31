# Instrument Hub — Handover

**What this is:** a JUCE 8 / C++20 audio engine with an embedded React UI,
controlled by a Novation **Launchpad Mini MK3** and a local HTTP + WebSocket
API.

**State at handover:** the application is **composed**. At startup `Main`
builds an eight-slot `Session` (fixed `AudioEngine` + `SlotSwitcher`), loads
the default init-tone set, opens the configured audio device and arms slot 0.
The control API is wired to the live session, and the Launchpad driver plus
`shared/set.yaml` mapping are started when a controller is present. A pad press
now reaches audio; `tests/e2e/` proves it end to end on the ALSA Loopback.

The remaining gaps are about breadth, not composition: the shipped mapping
declares 64 pads/slots while the default session has eight, the `.ihub` set and
sampler/plugin browsers still have no UI, and no physical Launchpad or audio
interface has been tested. Section 2 is the blunt list.

| | |
|---|---|
| Repository | `/home/mkocevar/Projects/instrument-hub` |
| Commit at handover | T117 checkpoint (this commit) |
| Date | 2026-10-31 |
| Host verified on | Ubuntu 25.04, Linux 6.14.0-37, x86_64 |
| Test result | `100% tests passed, 0 tests failed out of 51` |
| Product / binary | Instrument Hub / `instrument-hub` |
| Controller | Novation Launchpad Mini MK3 (64 pads, no keys, no encoders) |
| Licence | JUCE AGPLv3 unless a commercial licence is bought (B006) |

Read `plan/DECISIONS.md` before changing architecture, `plan/TESTING.md` for
the hardware-free recipes, and `plan/BLOCKERS.md` for the live human queue.

---

## 1. What works

### 1.1 Application shell and UI

- Native 1200×800 JUCE window. Release builds embed the React bundle through
  `juce::WebBrowserComponent` and serve it from a custom resource provider;
  Debug builds load the Vite dev server (`scripts/dev.sh`).
- React + TypeScript + Vite UI with:
  - 8×8 instrument grid, armed highlighting, per-slot and master meters,
    loading skeleton, 44 px+ touch targets;
  - audio settings panel (device type/device, sample rate, buffer size,
    channel mapping, latency readout, xrun counter, Measure button);
  - LAN pairing panel with a real QR code, 6-digit code and token storage;
  - `/remote` stage route: set list, previous/next patch, play/pause/stop,
    panic, reconnect banner, wake lock, high-contrast theme;
  - polling state store with exponential-backoff reconnect and optimistic arm.
- Screenshot/snapshot harness: `--snapshot out.png` captures the real native
  window, validates 1200×800 and non-blank content, and is covered by the
  `ihub-ui-snapshot` test.

### 1.2 Control API

Runs on `127.0.0.1:8080` by default; `--serve` runs it headless.

- `GET /state`, `POST /arm`, `GET/POST /settings`, `GET/POST /transport`
- `GET /audio`, `POST /audio`, `POST /audio/measure`
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

- `Session`: eight fixed slots, all wired in parallel into a summing bus at
  startup. Arming changes which slots process and applies a 3 ms equal-power
  gain ramp; nothing is rewired at runtime.
- `AudioEngine`: master limiter, no allocation/locks/logging in
  `processBlock()`.
- `Slot` / `FxChain` / `SlotSwitcher` / `WarmPool`: instruments → ordered FX →
  bus, 3 ms crossfades, warm-slot preload/LRU eviction.
- `RtGuard`: allocation/lock/log detector, `--rt-strict`, fork-isolated test.
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
- `Main::startSession()` builds the default eight-slot set, opens the device
  (`--audio-type`, `--audio-device`, `--audio-sample-rate`,
  `--audio-buffer-size`) and arms slot 0. The app makes sound on a fresh
  checkout with no samples or plugins.
- `Session::measureSwitch()` renders the armed slot, switches, and compares the
  result with the ideal 3 ms equal-power crossfade. `--session-smoke` runs it
  and then streams the same graph through the ALSA Loopback.
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
- Tests: protocol tables, all 64 pad bindings, scripted driver event/LED stream,
  hot reload, and the pad → session end-to-end path.

Full reference: `docs/launchpad-mini-mk3.md`. The old Launchkey document is
kept only as a superseded historical note.

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

### 1.8 Test suite

`ctest --test-dir build --output-on-failure` runs 51 tests. They cover audio
smoke and loopback latency, device hot-swap, graph mixing, RT guard, switch
continuity, warm pool, EQ/compressor/delay/reverb, transport/clock/tap, plugin
scan/cache/instance/editor/FX/sandbox, Launchpad protocol/mapping/driver,
the composed session and the API behind it, `.ihub` set/migration/plugin state,
sample loader/kit/sampler, diagnostics, the UI snapshot, and `ihub-e2e`.

`ihub-e2e` is the T117 acceptance check. It sends a Launchpad pad event through
`LaunchpadDriver` → `Mapping` → `Session::armSlot` into both the offline render
and a live ALSA Loopback stream, and records:

```console
$ ctest --test-dir build -R ihub-e2e --output-on-failure
...
  switch measurement: rms=0.082524 rms_dbfs=-21.6684 discontinuity_dbfs=-154.609 ...
  loopback: playback="hw:CARD=Loopback,DEV=0" capture="hw:CARD=Loopback,DEV=1" \
            captured_first_rms_dbfs=-21.5549 captured_second_rms_dbfs=-21.4595 ...
  record: pads=0->3 render_rms=0.082524 render_rms_dbfs=-21.6684 \
          switch_discontinuity_dbfs=-154.609 loopback_rms_dbfs_first=-21.5549 \
          loopback_rms_dbfs_second=-21.4595
100% tests passed, 0 tests failed out of 1
```

The rendered RMS is non-silent, the switch residual is far below -60 dBFS, and
a hard-switch control with the ramp disabled (0 ms) produces a 0.11186 step, so
the click detector is proven able to fail. The test also identifies the audible
tone in each window (slot 0 is 110 Hz, slot 3 is 146.83 Hz), so the slot the pad
armed is demonstrably the slot that is heard. All 51 tests passed on this host
at the T117 checkpoint.

---

## 2. What is stubbed / not yet wired

The app now composes and plays a session. These are the remaining gaps, in the
order that matters for a usable instrument.

1. **The mapping is 64 pads; the session is 8 slots.** `shared/set.yaml`
   declares 64 pads and 64 slots (8 families × 8 patches), but
   `Session::makeDefaultSet()` builds eight init tones, and `Main` gives the
   API an eight-slot `StateStore`. Pads 0..7 (the Keys row) arm audible audio;
   pads 8..63 resolve in the mapping and are then ignored because their slot is
   outside the graph. Fix by building a 64-slot session (a 64-slot `.ihub` set
   or a larger default set) and sizing `StateStore` from the session.
2. **The default set is eight init tones.** Sampler, plugin and audio-input
   slots are implemented in `Session::build()`, but the app never chooses one.
   Loading a real kit or VST3 into a slot is a code/CLI path, not a UI action.
3. **The set/project layer has no UI or API.** There is no Open/Save `.ihub`
   action, no set browser, and no `GET/POST /set` endpoint. The serializer and
   migration are exercised by tests only.
4. **Sampler and plugin browsers have no UI.** Kits can be loaded by
   code/tests; the running UI exposes only the slot grid. Plugin scanning is a
   CLI (`--scan-plugins`), not a user-facing browser.
5. **The grid is flat.** `ui/README.md` documents that `/state` carries flat
   slots, so `gridFromState()` maps each slot to a one-patch family. The view
   is family/patch-shaped, but the real family tree is not in the API yet.
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

### The shortest path to the full 8×8 set

1. Make the composed session own the 64 slots the mapping names (default set
   or loaded `.ihub`), and let `StateStore` follow `Session::getNumSlots()`.
2. Load the mapping's family/instrument names into the session slots so
   `/state`, the grid and the remote show the real 8×8 tree.
3. Expose `.ihub` open/save through the API and add the UI actions.
4. Add kit and plugin selection per slot (the factories already exist).
5. Run the hardware checks in section 4.

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
`snd-virmidi`, checks the toolchain and then builds and tests. If the machine
is already set up, `scripts/bootstrap.sh --check` is read-only.

Build and test:

```bash
scripts/build.sh                 # RelWithDebInfo into build/
ctest --test-dir build --output-on-failure
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
./build/engine/instrument-hub --selftest          # window for 3 s, exit 0
./build/engine/instrument-hub --session-smoke     # composed graph + ALSA Loopback
./build/engine/instrument-hub --list-devices
./build/engine/instrument-hub --measure-latency
./build/engine/instrument-hub --snapshot out.png
./build/engine/instrument-hub --ui-check
./build/engine/instrument-hub --diagnostics
./build/engine/instrument-hub --rt-strict
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

Run modes are the same flags as Linux. The `--rt-strict` linker wrapping and
the ALSA Loopback/controller acceptance tests are Linux-only; the app builds
and runs without them on Windows.

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
`--list-devices` and a real `--selftest` under Xvfb.

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

B002 is retained in `BLOCKERS.md` as history: it describes the superseded
Launchkey surface and must not be used as the controller checklist.

---

## 5. Open items copied from `plan/BLOCKERS.md`

`B001` is **resolved** (C++ headers installed). The following are **open** at
handover and must be closed by a human:

- **B009 · Launchpad Mini MK3 physical verification** (supersedes B002) — the
  protocol, driver and mapping are tested against scripted MIDI; real firmware
  may disagree on programmer-mode entry, LED colours or port behaviour.
  `status: open`, `severity: soft`. Human: connect the Launchpad and run the
  checklist in section 4.
- **B002 · Launchkey Mini MK3 physical verification** — historical, superseded
  by B009. `status: open`, `severity: soft`. Do not action this entry; the app
  does not start the Launchkey driver.
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

## 6. Verification actually run for this handover

Environment: Ubuntu 25.04, Linux 6.14.0-37-generic, x86_64, CMake 3.31.6,
Ninja 1.12.1, GCC 14.2.0, Node.js v22.23.3, pnpm 11.24.0.

Build:

```console
$ scripts/build.sh
...
✓ built in 136ms
...
built: /home/mkocevar/Projects/instrument-hub/build/engine/instrument-hub
```

Test suite:

```console
$ ctest --test-dir build --output-on-failure
...
100% tests passed, 0 tests failed out of 51
Total Test time (real) =  34.30 sec
```

End-to-end acceptance:

```console
$ ctest --test-dir build -R ihub-e2e --output-on-failure
...
  record: pads=0->3 render_rms=0.082524 render_rms_dbfs=-21.6684 \
          switch_discontinuity_dbfs=-154.609 loopback_rms_dbfs_first=-21.5549 \
          loopback_rms_dbfs_second=-21.4595
1/1 Test #45: ihub-e2e .........................   Passed
```

Device path:

```console
$ ./build/engine/instrument-hub --session-smoke
session-smoke: built 8 slots
session-smoke: armed_slot=0 rms=0.082524 rms_dbfs=-21.6684 switch_to=1 \
               discontinuity_dbfs=-154.196 ...
session-smoke: device="Loopback, Loopback PCM; Direct hardware device without \
               any conversions (1)" rate=48000 block=128 callbacks=...
session-smoke: PASS
```

Additional historical evidence (from the task result files in
`plan/.autopilot/`): Release UI snapshot 1200×800 with 421 distinct colours;
loopback latency measurement `6.33 ms / 304 samples / confidence 100%`;
sandbox crash isolation; AppImage executed with `--list-devices` and
`--selftest`. Those checks are not re-run by this document; their exact
commands are in the corresponding `.result` files.

### 6.1 ALSA Loopback note

`plan/TESTING.md` says "card 2"; on this host the `snd-aloop` card enumerated
as **card 3** after the UR22mkII. Always find it by name (`aplay -l | grep -i
loopback`, or `hw:CARD=Loopback,...`), never by card index. Playback is
`hw:CARD=Loopback,DEV=0`, capture is `hw:CARD=Loopback,DEV=1`. The T117 test
does exactly that.

---

## 7. Repo map

```
engine/src/
  Main.cpp              application shell, CLI modes, web view, API, session,
                        Launchpad + mapping startup
  Session.{h,cpp}       composed eight-slot session, device ownership, switch
                        measurement, real meters
  api/                  HTTP + WebSocket server, state store, session bridge,
                        pairing, mDNS
  audio/                engine, slots, bus, device manager, transport, clocks,
                        latency probe, timing monitor, RT guard
  controller/           Launchpad protocol/driver, set.yaml mapping; historical
                        Launchkey protocol/driver for its old tests
  dsp/                  EQ, compressor, delay, reverb
  instruments/          Instrument interface, audio input, plugin FX
  plugins/              scanner, cache, plugin instance/window, sandbox
  project/              .ihub set format, serializer, migrations, plugin state
  sampler/              sample loader, kit format, polyphonic sampler
  diag/                 log ring, crash dump, diagnostics report
  Bridge/               web view and embedded-asset provider
shared/                 openapi.yaml, schemas, set.yaml
ui/                     React app (grid, audio settings, pairing, /remote)
tests/                  51 ctest targets; tests/e2e/ is the pad-to-audio
                        acceptance
scripts/                build.sh, dev.sh, bootstrap.sh
plan/                   roadmap, decisions, testing, blockers, autopilot
docs/                   Launchpad reference, new-machine setup
```

Start with `plan/AUTOPILOT.md` for the session protocol, then section 2 above
for the remaining integration work, then the relevant module and its test.
