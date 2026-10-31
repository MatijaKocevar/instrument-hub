# Instrument Hub — Handover

**What this is:** a JUCE 8 / C++20 audio engine with an embedded React UI,
controlled by a Novation Launchkey Mini MK3 and a local HTTP + WebSocket API.

**State at handover:** every task in `plan/ROADMAP.md` is implemented and the
Linux test suite is green (44/44), but the shipping application is not yet
composed into a playable instrument. The engine modules, controller driver,
sampler, project format and plugin host all exist and are individually tested;
the app shell starts the UI, the control API and the audio device settings, and
stops there. Section 2 is the important part for whoever picks this up.

| | |
|---|---|
| Repository | `/home/mkocevar/Projects/instrument-hub` |
| Commit at handover | `670cdee` (`autopilot: T112 Packaging`) |
| Date | 2026-10-31 |
| Host verified on | Ubuntu 25.04, Linux 6.14, x86_64 |
| Test result | `100% tests passed, 0 tests failed out of 44` |
| Product / binary | Instrument Hub / `instrument-hub` |
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
- `GET /state` and `/events` expose slot names, armed state, per-slot chain
  latency, transport and settings.

### 1.3 Audio path and DSP (modules implemented and tested)

- `DeviceManager`: enumeration, open/close, hot-swap, buffer-size changes,
  channel masks, input/output latency. The app's audio settings panel drives
  this real device path.
- `AudioEngine`: fixed pre-allocated slot graph summed into a bus with a master
  limiter; no allocation/locks/logging in `processBlock()`.
- `Slot` / `FxChain` / `SlotSwitcher` / `WarmPool`: instruments → ordered FX →
  bus, 3 ms equal-power crossfades, warm-slot preload/LRU eviction.
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

### 1.4 Instruments, plugins and sampler (modules implemented and tested)

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

### 1.5 Controller (module implemented and tested)

- Pure `LaunchkeyProtocol`: session-mode SysEx, pad map, LED palette,
  encoder CCs, key range.
- ALSA virtual Launchkey simulator named `Launchkey Mini MK3 SIM`, fixture
  replay and LED recording.
- `LaunchkeyDriver`: device discovery, handshake, pad/encoder/key callbacks,
  LED feedback.
- `Mapping`: `set.yaml` loader with families/pads/encoders/macros, validation
  and hot reload.
- End-to-end driver integration test compares the recorded LED stream against a
  golden file.

### 1.6 Project, diagnostics and packaging

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

### 1.7 Test suite

`ctest --test-dir build --output-on-failure` runs 44 tests, including:
audio smoke and loopback latency, device hot-swap, graph mixing, RT guard,
switch continuity, warm pool, EQ/compressor/delay/reverb, transport/clock/tap,
plugin scan/cache/instance/editor/FX/sandbox, Launchkey protocol/driver/mapping
integration, API/pairing/state broadcast/discovery, `.ihub` set/migration/
plugin state, sample loader/kit/sampler, diagnostics, and the UI snapshot.
All 44 passed on this host at the handover commit.

---

## 2. What is stubbed / not yet wired

This section is deliberately blunt. The modules above are real and tested in
isolation; the following gaps are why the app is not yet a usable instrument.

1. **The application does not compose a session.** `engine/src/Main.cpp`
   creates the UI, `Api::StateStore`, `Api::Server` and
   `AudioSettingsBridge` only. It never constructs an `AudioEngine`, `Transport`,
   `LaunchkeyDriver`, `Mapping`, `SamplerInstrument`, `SetFile` or
   `PluginStateStore`. There is no audio graph running in the normal app, so
   the app makes no sound even with an instrument selected.
2. **Arm / settings / transport requests only change the in-memory state.**
   `StateStore::setArmHandler()` and `setTransportHandler()` are never called
   in `Main.cpp`; `POST /arm` moves the UI highlight and `POST /transport`
   changes the reported values, but neither reaches a slot switcher or a real
   transport. `POST /settings` is likewise a value store.
3. **Meters are synthetic.** The `/meters` default frame reports
   `gain = 1.0` for the armed slot and `0.0` otherwise, with no audio data.
   No real meter provider is installed by the app.
4. **The controller is not started by the app.** The Launchkey driver exists
   and passes its simulator tests, but no app code opens the MIDI device,
   loads `shared/set.yaml`, or forwards pads/keys/encoders.
5. **The set/project layer has no UI or API.** There is no Open/Save `.ihub`
   action, no set browser, and no `GET/POST /set` endpoint. The serializer and
   migration are exercised by tests only.
6. **Sampler and plugin browser have no UI.** Kits can be loaded by code/tests;
   the running UI exposes only the 8×8 slot grid. Plugin scanning is a CLI
   (`--scan-plugins`), not a user-facing browser.
7. **The grid is flat.** `ui/README.md` documents that `/state` carries flat
   slots, so `gridFromState()` maps each slot to a one-patch family. The view is
   family/patch-shaped, but the real family tree is not in the API yet.
8. **mDNS is POSIX-only.** `Discovery::start()` returns
   "mDNS discovery is not implemented on this platform" on Windows. Linux has
   been verified against Avahi; macOS is untested.
9. **Missing plugins become silent placeholders by design.** This is intended
   (T081) and reported as a warning, not a crash, but a set referencing an
   absent plugin will not make sound for that slot.
10. **Third-party plugins are not validated.** `TestPlugin` covers the host
    contract. Analog Lab Pro and EZdrummer 3 need a licensed Windows/macOS
    machine (B003).
11. **Packaging is unsigned.** The MSI, DMG and AppImage have no signature,
    notarisation or timestamp. Windows/macOS packages are compile-only until
    B005 is done.
12. **No hardware tuning or physical controller run.** All latency/xrun
    figures come from the ALSA loopback; all MIDI runs use the virtual
    Launchkey. See section 4.

### The shortest path to a playable app

The interfaces and tests are in place; what remains is composition, not new
DSP:

1. Add a session class owning `AudioEngine`, `Transport`, `WarmPool`,
   `PluginStateStore`, `LaunchkeyDriver` and `Mapping`, created in
   `Main.cpp` before the API starts.
2. Install the state-store handlers: arm → `SlotSwitcher::armSlot()`,
   transport → `Transport`, settings → `DeviceManager`/engine, meters →
   `AudioEngine` level taps.
3. Attach the audio callback (`AudioSettingsBridge`/`DeviceManager::
   setAudioCallback`) to the session, and load the default set / `set.yaml` at
   startup.
4. Expose `.ihub` open/save through the API and add the corresponding UI
   actions.
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
./build/engine/instrument-hub              # window + control API
./build/engine/instrument-hub --serve      # headless API
./build/engine/instrument-hub --audio      # open the ALSA Loopback device
```

Useful one-shot modes:

```bash
./build/engine/instrument-hub --selftest          # window for 3 s, exit 0
./build/engine/instrument-hub --list-devices
./build/engine/instrument-hub --measure-latency
./build/engine/instrument-hub --snapshot out.png
./build/engine/instrument-hub --ui-check
./build/engine/instrument-hub --diagnostics
./build/engine/instrument-hub --rt-strict
```

`--api-port`, `--api-host`, `--api-token` and `--lan` change the control
server. JACK warnings on a machine without JACK are harmless; ALSA still
enumerates.

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

Run modes are the same flags as Linux. The `--rt-strict` linker wrapping is
Linux-only; the app builds and runs without it on Windows.

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
| B002 | Novation Launchkey Mini MK3 | Connect the controller, run the driver, confirm (a) pads light in the expected colours, (b) pads report in session mode, (c) encoders move macros, (d) keys reach the armed instrument. Record any byte mismatch; tables live in `LaunchkeyProtocol.cpp`. **Needs the app composition from section 2 first, otherwise there is nothing for the keys to reach.** | Virtual `Launchkey Mini MK3 SIM` + golden LED stream |
| B003 | Windows music PC with Analog Lab Pro and EZdrummer 3 | Load each in Instrument Hub, confirm the editor opens, presets load, state saves and reloads, and record reported latency. | In-repo `TestPlugin.vst3`; an Analog Lab V cache entry may exist but has not been validated |
| B004 | Real audio interface | Connect the interface, try 64/128/256 frames at 48 kHz, record xrun counts and measured round-trip, and update the default in `DECISIONS.md` if it differs. An interface may enumerate (the current host lists a Steinberg UR22mkII) but no measurements exist. | ALSA Loopback figures are a lower bound, not a promise |
| B005 | Windows and macOS machines | Run the CI artifacts on each OS; confirm the window opens, an audio device enumerates and plugin scanning works. | CI compile success only |
| B007 | Tablet | Scan the QR from the tablet, confirm pairing, set list, tempo control, and that Launchkey pad presses highlight on the tablet. | Responsive browser snapshots at tablet viewports |

Non-hardware open items from `BLOCKERS.md`:

- **B006 · JUCE licence decision** — decide AGPLv3 (publish the engine source)
  or buy a commercial JUCE licence before distributing a closed-source binary.
  Private use is unaffected.
- **B008 · Chinese public holidays** — the autopilot's off-peak scheduler treats
  Chinese public holidays as normal weekdays; add dates to `HOLIDAYS` in
  `autopilot.py` if a run lands on one. Does not affect the product.

Also worth repeating: the controller and audio interface checks above cannot
be completed meaningfully until section 2's session wiring exists. The
hardware-free tests prove the modules, not the composed instrument.

---

## 5. Open items copied from `plan/BLOCKERS.md`

`B001` is **resolved** (C++ headers installed). The following are **open** at
handover and must be closed by a human:

- **B002 · Launchkey Mini MK3 physical verification** — LED palette velocities
  and session-mode SysEx are implemented from the Programmer's Reference and
  tested against a simulator; real firmware may disagree.
  `status: open`, `severity: soft`. Human: connect the Mini MK3 and confirm pad
  colours, session-mode reporting, encoder macros and keys reaching the armed
  instrument.
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
  pad-press highlighting.
- **B008 · Chinese public holidays** — off-peak scheduling treats Chinese public
  holidays as normal weekdays. `status: open`, `severity: soft`. Human: add
  dates to `HOLIDAYS` in `autopilot.py` if a run lands on one.

---

## 6. Verification actually run for this handover

Environment at the time: Ubuntu 25.04, Linux 6.14.0-37-generic, x86_64,
CMake 3.31.6, Ninja 1.12.1, GCC 14.2.0, Node.js v22.23.3, pnpm 11.24.0,
commit `670cdee`.

Build (the acceptance command for T113):

```console
$ scripts/build.sh
...
✓ built in 173ms
-- Configuring done
-- Generating done
-- Build files have been written to: /home/mkocevar/Projects/instrument-hub/build
...
built: /home/mkocevar/Projects/instrument-hub/build/engine/instrument-hub
```

Test suite:

```console
$ ctest --test-dir build --output-on-failure
...
100% tests passed, 0 tests failed out of 44
Total Test time (real) =  32.83 sec
```

Device path:

```console
$ ./build/engine/instrument-hub --list-devices
audio device types: ALSA
...
device: Loopback ...
exit=0
```

Additional historical evidence (from the task result files in
`plan/.autopilot/`): Release UI snapshot 1200×800 with 421 distinct colours;
loopback latency measurement `6.33 ms / 304 samples / confidence 100%`;
Launchkey integration and state broadcast under 6 ms; sandbox crash isolation;
AppImage executed with `--list-devices` and `--selftest`. Those checks are not
re-run by this document; their exact commands are in the corresponding
`.result` files.

---

## 7. Repo map

```
engine/src/
  Main.cpp              application shell, CLI modes, web view, API startup
  api/                  HTTP + WebSocket server, state store, pairing, mDNS
  audio/                engine, slots, bus, device manager, transport, clocks,
                        latency probe, timing monitor, RT guard
  controller/           Launchkey protocol, driver, set.yaml mapping
  dsp/                  EQ, compressor, delay, reverb
  instruments/          Instrument interface, audio input, plugin FX
  plugins/              scanner, cache, plugin instance/window, sandbox
  project/              .ihub set format, serializer, migrations, plugin state
  sampler/              sample loader, kit format, polyphonic sampler
  diag/                 log ring, crash dump, diagnostics report
  Bridge/               web view and embedded-asset provider
shared/                 openapi.yaml, schemas, set.yaml
ui/                     React app (grid, audio settings, pairing, /remote)
tests/                  44 ctest targets and fixtures
scripts/                build.sh, dev.sh, bootstrap.sh
plan/                   roadmap, decisions, testing, blockers, autopilot
docs/                   Launchkey reference, new-machine setup
```

Start with `plan/AUTOPILOT.md` for the session protocol, then section 2 above
for the integration work, then the relevant module and its test.
