# Architecture

This document is the map of Instrument Hub: what each directory owns, which
way the dependencies point, and the four call paths that matter when something
stops working. It is written for a contributor who has the repository checked
out and wants to make a change without breaking the audio thread.

The short version:

- `engine/src/Session.cpp` owns the live graph; `engine/src/Main.cpp` owns the
  process and wires everything else around it.
- The fixed graph is built once. Arming a patch never rewires it; it changes a
  lock-free pointer and an atomic switch request.
- The UI is a web view embedded in the app. It talks HTTP and WebSocket, never
  C++.
- The controller is an external MIDI device. A pad resolves to a slot through
  `shared/set.yaml`, then follows the same arm path as the UI.
- The audio callback allocates, locks and logs nothing. That rule is not a
  convention; `RtGuard` enforces it in strict mode, and it is the one invariant
  every change has to preserve.

Line counts and file names below are those of the T133 checkpoint. The code is
the authority; this document explains why it is shaped the way it is.

---

## 1. Start reading here

Read in this order. It is deliberately small: roughly one focused morning.

| Step | Read | Why |
|---|---|---|
| 1 | [plan/DECISIONS.md](../plan/DECISIONS.md) and [AGENTS.md](../AGENTS.md) | The locked decisions and the audio-thread contract. Everything else assumes them. |
| 2 | [engine/src/Session.h](../engine/src/Session.h), class comment only | The model in one page: 64 logical slots, a few live instruments, warm pool, threading rules. |
| 3 | [engine/src/Main.cpp](../engine/src/Main.cpp): `startSession()`, `startControlApi()`, `startController()` | How the process is composed. Note which object is created first and why. |
| 4 | [engine/src/Session.cpp](../engine/src/Session.cpp): `Session::build()`, `armSlot()`, `finishArmLocked()`, `warmCallback()`, `renderInternal()`, `audioDeviceIOCallbackWithContext()` | The control path and the audio path in one file. |
| 5 | [engine/src/audio/SlotSwitcher.h](../engine/src/audio/SlotSwitcher.h), [Slot.h](../engine/src/audio/Slot.h), [FxChain.h](../engine/src/audio/FxChain.h), [Bus.h](../engine/src/audio/Bus.h) | What actually renders a block, and where the equal-power crossfade lives. |
| 6 | [engine/src/audio/WarmPool.h](../engine/src/audio/WarmPool.h) | The instance cache, its thread, its limit and its lifetime rules. |
| 7 | [engine/src/api/StateStore.h](../engine/src/api/StateStore.h) (`StateStore`), then [engine/src/api/SessionBridge.cpp](../engine/src/api/SessionBridge.cpp) | How HTTP reaches the session without the route code knowing what a `Session` is. |
| 8 | [engine/src/controller/LaunchpadProtocol.h](../engine/src/controller/LaunchpadProtocol.h), then [LaunchpadDriver.h](../engine/src/controller/LaunchpadDriver.h), then [Mapping.h](../engine/src/controller/Mapping.h) | The pad path, from a raw note to a slot number. |
| 9 | [engine/src/Session.cpp](../engine/src/Session.cpp): `capturePluginState()`, `writePluginStateFile()` in the anonymous namespace, `applyStoredPluginState()` | Per-patch sound persistence, including the explicit save action. |
| 10 | [tests/e2e/PadToAudio.cpp](../tests/e2e/PadToAudio.cpp), [tests/session_warm/SessionWarmPoolTest.cpp](../tests/session_warm/SessionWarmPoolTest.cpp), [tests/api/SessionApiTest.cpp](../tests/api/SessionApiTest.cpp) | The executable version of this document. |

Useful references while reading:

- [docs/launchpad-mini-mk3.md](launchpad-mini-mk3.md) — controller protocol and
  mapping.
- [plan/TESTING.md](../plan/TESTING.md) — how to prove behaviour without
  hardware (ALSA Loopback, virtual Launchpad, snapshots).
- [shared/api/openapi.yaml](../shared/api/openapi.yaml) — the HTTP contract the
  UI's TypeScript schema is generated from.
- [HANDOVER.md](../HANDOVER.md) — the current state, known gaps and run modes.

If you only read one source file, read `Session.h`. If you only read one
function, read `SlotSwitcher::process()`.

---

## 2. The shape in one page

`Main.cpp` is the composition root. It owns the objects below and wires them
with lambdas; no other translation unit constructs the application.

```
Main.cpp  (composition root: lifecycle, CLI modes, window, LED poll)
  |
  +-- Session          fixed graph, warm pool, device, per-patch state
  |     |
  |     +-- audio/     Slot, SlotSwitcher, Bus, WarmPool, DeviceManager,
  |     |              Transport, clocks, timing, latency, RtGuard
  |     |     +-- dsp/ EQ, compressor, delay, reverb (leaf)
  |     |
  |     +-- sampler/   Kit, SampleLoader, SamplerInstrument
  |     +-- plugins/   PluginInstrument, inventory, scanner, sandbox, editor
  |     +-- instruments/ Instrument seam, AudioInputInstrument, PluginFx
  |
  +-- api/             StateStore, HTTP + WebSocket server, routes,
  |                    SessionBridge / PluginBridge / MidiBridge, pairing
  |
  +-- controller/      Launchpad protocol + driver, set.yaml mapping
  +-- Bridge/          WebView host (dev server or embedded ui/dist)
  +-- diag/            log ring, crash dump, diagnostics report

ui/ (React)  <--- HTTP + WebSocket --->  api/       (the UI knows no C++)
project/     .ihub container library (above audio/plugins; tests only today)
```

The UI is a web view. It talks HTTP and WebSocket, never C++. The controller is
an external MIDI device; its driver decodes notes and calls a listener, never
`Session` directly.

Runtime topology (threads and processes):

- **Message thread** — JUCE application, window/web view, `Main` timers (LED
  poll, snapshot), plugin editor windows, plugin destruction posted by the
  warm pool.
- **HTTP worker threads** — the `cpp-httplib` worker pool; route handlers and
  `SessionBridge` handlers run here. They are control threads, never audio.
- **WebSocket worker threads** — one per `/meters` or `/events` client, plus
  the server's meter thread that produces frames at 30 Hz.
- **Controller MIDI thread** — JUCE MIDI input callback; the Launchpad driver
  decodes and dispatches. The mapping watcher has its own thread for hot
  reload (`Mapping::watchLoop`).
- **Warm-pool worker thread** — one per `WarmPool`; creates and evicts
  instruments. Never the audio thread, never the message thread.
- **Audio device callback thread** — the only real-time thread. It runs
  `Session::audioDeviceIOCallbackWithContext()`.
- **Helper processes** — `ihub-scanner` (one VST3 bundle per run, result passed
  by file) and `ihub-sandbox` (optional per-plugin isolation, audio passed
  through the documented shared-memory mapping). Neither can take the engine
  down with it.

---

## 3. Module responsibilities

Every directory under `engine/src` is listed here. The one-line rule for each
is followed by the types worth knowing.

### 3.1 `engine/src/` (root)

- [Main.cpp](../engine/src/Main.cpp) — application entry point, JUCE
  application lifecycle, CLI modes (`--serve`, `--selftest`, `--snapshot`,
  `--diagnostics`, `--session-smoke`, `--list-devices`, `--rt-strict`,
  `--measure-latency`, `--scan-plugins`, `--ui-check`), default-set/source
  loading, API and controller composition, the 100 ms LED mirror, and the
  native window. If a subsystem is created, it is created here. There is no
  other composition root in the app.
- [Session.h](../engine/src/Session.h) / [Session.cpp](../engine/src/Session.cpp)
  — the composed audio session: owns the fixed `AudioEngine`, the
  `SlotSwitcher`, the `WarmPool`, the per-slot `FxRack`s, the `DeviceManager`,
  the `Transport`, the editor windows and the per-patch plugin-state sidecar.
  Implements `juce::AudioIODeviceCallback`. The header's class comment is the
  threading contract; read it before changing anything here.

### 3.2 `engine/src/api/`

The control surface. It is a facade over `StateStore`; only `SessionBridge`
knows what a `Session` is.

- [StateStore.h](../engine/src/api/StateStore.h) /
  [StateStore.cpp](../engine/src/api/StateStore.cpp) — `StateStore` (the
  UI-facing mirror: armed slot, settings, transport, family tree, per-slot
  mix, latency, editor flags) and the plain structs it projects.
- [Server.h](../engine/src/api/Server.h) / [Server.cpp](../engine/src/api/Server.cpp)
  — `Server` (cpp-httplib listener lifecycle, LAN rebind, CORS, pairing and
  discovery ownership). Authentication, meter sampling and the WebSocket
  endpoints are delegated to the modules below.
- [AuthGuard.h](../engine/src/api/AuthGuard.h) /
  [AuthGuard.cpp](../engine/src/api/AuthGuard.cpp) — the token/LAN policy
  checked before every route and WebSocket upgrade; loopback-only pairing
  management and the `X-Instrument-Hub-Token` / bearer rules.
- [MeterSampler.h](../engine/src/api/MeterSampler.h) /
  [MeterSampler.cpp](../engine/src/api/MeterSampler.cpp) — samples the meter
  provider at `meterHz` on its own thread and serialises the newest frame.
- [WebSocketRoutes.h](../engine/src/api/WebSocketRoutes.h) /
  [WebSocketRoutes.cpp](../engine/src/api/WebSocketRoutes.cpp) — installs
  `/meters` and `/events`, coalesces frames per client and tracks open
  connections for shutdown.
- [Routes.h](../engine/src/api/Routes.h) / [Routes.cpp](../engine/src/api/Routes.cpp)
  — the route table built by `registerRoutes()`, plus the JSON helpers and the
  public JSON serialisers. Each resource lives in its own small module under
  [routes/](../engine/src/api/routes/): routes by resource and the matching
  state/audio/MIDI/pairing/meter serialisers in their own translation units.
  A module only sees a `RouteContext` of function objects, so no route links
  against `Session`.
- [SessionBridge.h](../engine/src/api/SessionBridge.h) /
  [SessionBridge.cpp](../engine/src/api/SessionBridge.cpp) — the adapter that
  installs live handlers on `StateStore`: arm, transport, patch mix/source,
  editor, state capture, FX chain, runtime slot state, meters. This is the only
  API translation unit that includes `Session.h`.
- [PluginBridge.h](../engine/src/api/PluginBridge.h) /
  [PluginBridge.cpp](../engine/src/api/PluginBridge.cpp) — installs the VST3
  inventory and rescan handlers behind `/plugins`.
- [MidiBridge.h](../engine/src/api/MidiBridge.h) /
  [MidiBridge.cpp](../engine/src/api/MidiBridge.cpp) — MIDI input enumeration,
  input enable/disable, the Launchpad 64-pad test, and the `MidiSink` that
  feeds `Session::pushMidi()`.
- [Pairing.h](../engine/src/api/Pairing.h) / [Pairing.cpp](../engine/src/api/Pairing.cpp)
  — 6-digit pairing codes, token storage/revocation and LAN-mode state.
- [Discovery.h](../engine/src/api/Discovery.h) / [Discovery.cpp](../engine/src/api/Discovery.cpp)
  — a self-contained mDNS/DNS-SD responder for `_instrumenthub._tcp`; POSIX
  only, started with LAN mode.
- [StateBroadcast.h](../engine/src/api/StateBroadcast.h) /
  [StateBroadcast.cpp](../engine/src/api/StateBroadcast.cpp) — coalesced
  state frames for `/events`; every connected client sees the newest frame,
  not a backlog.

### 3.3 `engine/src/audio/`

The real-time core. Nothing here knows about the UI or the controller; the one
API dependency (`AudioSettings`) is the exception noted in §4.3.

- [AudioEngine.h](../engine/src/audio/AudioEngine.h) /
  [AudioEngine.cpp](../engine/src/audio/AudioEngine.cpp) — fixed array of
  `Slot`s, summing `Bus`, timing monitor, MIDI device buffer. Its
  `processBlock()` is the standalone engine path used by tests and offline
  tools; the app's `Session` drives `SlotSwitcher` and `Bus` directly.
- [Slot.h](../engine/src/audio/Slot.h) / [Slot.cpp](../engine/src/audio/Slot.cpp)
  — one graph slot: `SlotSource` → ordered `FxChain` → gain → bus. Owns its
  render buffer and its own 2–5 ms activation ramp. `processUnfaded()` is the
  path `SlotSwitcher` uses; `process()` is the older single-slot path.
- [SlotSwitcher.h](../engine/src/audio/SlotSwitcher.h) /
  [SlotSwitcher.cpp](../engine/src/audio/SlotSwitcher.cpp) — the armed-slot
  crossfade. `armSlot()` is one atomic store; `process()` turns it into a
  3 ms equal-power fade across all slots.
- [FxChain.h](../engine/src/audio/FxChain.h) / [FxChain.cpp](../engine/src/audio/FxChain.cpp)
  — ordered non-owning `juce::AudioProcessor*` chain. Empty chain is a
  bit-exact passthrough.
- [FxRack.h](../engine/src/audio/FxRack.h) / [FxRack.cpp](../engine/src/audio/FxRack.cpp)
  — owns the insert processors for one patch, builds built-ins from `dsp/`
  and loads hosted VST3 inserts through its `AudioPluginFormatManager`,
  exposes snapshots and parameter edits, and rebuilds the non-owning
  `FxChain` after an edit. The insert wrapper, factory, parameter mapping,
  editing, ordering, latency and snapshot code live in
  [audio/fx/](../engine/src/audio/fx/), so the rack file keeps only chain
  wiring and transport forwarding.
- [Bus.h](../engine/src/audio/Bus.h) / [Bus.cpp](../engine/src/audio/Bus.cpp)
  — master summing buffer and zero-latency peak limiter. Unity below threshold.
- [WarmPool.h](../engine/src/audio/WarmPool.h) /
  [WarmPool.cpp](../engine/src/audio/WarmPool.cpp) — background instrument
  cache: protected set (active + K neighbours), hard `1 + K` live-instance
  limit, LRU eviction, pinning, deferred loads, destroy posted to the message
  thread. Slot reservation, warm-up queue, eviction/accounting and the worker
  thread live in [audio/warm/](../engine/src/audio/warm/), so the pool file
  keeps lifecycle, configuration and inspection.
- [DeviceManager.h](../engine/src/audio/DeviceManager.h) /
  [DeviceManager.cpp](../engine/src/audio/DeviceManager.cpp) — JUCE device
  enumeration, setup and open/close.
- [AudioSettings.h](../engine/src/audio/AudioSettings.h) /
  [AudioSettings.cpp](../engine/src/audio/AudioSettings.cpp) — the Audio page's
  device and latency bridge. See the layering note in §4.3.
- [Transport.h](../engine/src/audio/Transport.h) /
  [Transport.cpp](../engine/src/audio/Transport.cpp) — BPM, playing state,
  PPQ and sample position; `process()` advances it.
- [ClockIn.h](../engine/src/audio/ClockIn.h) / [ClockIn.cpp](../engine/src/audio/ClockIn.cpp)
  and [ClockOut.h](../engine/src/audio/ClockOut.h) /
  [ClockOut.cpp](../engine/src/audio/ClockOut.cpp) — 24 PPQN MIDI clock
  follower and generator.
- [TapTempo.h](../engine/src/audio/TapTempo.h) / [TapTempo.cpp](../engine/src/audio/TapTempo.cpp)
  — median-of-intervals tap tempo.
- [TimingMonitor.h](../engine/src/audio/TimingMonitor.h) /
  [TimingMonitor.cpp](../engine/src/audio/TimingMonitor.cpp) — callback
  deadline, overrun count and rolling maximum; audio-thread cheap.
- [LatencyProbe.h](../engine/src/audio/LatencyProbe.h) /
  [LatencyProbe.cpp](../engine/src/audio/LatencyProbe.cpp) — ALSA Loopback
  impulse/cross-correlation round-trip measurement.
- [RtGuard.h](../engine/src/audio/RtGuard.h) / [RtGuard.cpp](../engine/src/audio/RtGuard.cpp)
  — allocation/lock/log detector and `operator new` interposition for
  `--rt-strict`.
- [Instrument.h](../engine/src/audio/Instrument.h) — compatibility shim only;
  the canonical interface is `instruments/Instrument.h`.

### 3.4 `engine/src/Bridge/`

- [WebView.h](../engine/src/Bridge/WebView.h) / [WebView.cpp](../engine/src/Bridge/WebView.cpp)
  — the `juce::WebBrowserComponent` host. Debug builds load
  `http://localhost:5173`; release builds serve the three embedded `ui/dist`
  files from `BinaryData` and fall back to `index.html` for client-side routes.
  It contains no product logic.

### 3.5 `engine/src/controller/`

- [LaunchpadProtocol.h](../engine/src/controller/LaunchpadProtocol.h) /
  [LaunchpadProtocol.cpp](../engine/src/controller/LaunchpadProtocol.cpp) —
  pure data and pure functions: SysEx header/handshake, programmer mode, grid
  notes 11..88, top-row CCs 91..98, right-column CCs 89..19, palette and LED
  modes. No MIDI device, no allocation.
- [LaunchpadDriver.h](../engine/src/controller/LaunchpadDriver.h) /
  [LaunchpadDriver.cpp](../engine/src/controller/LaunchpadDriver.cpp) — MIDI
  I/O, handshake, pad/CC decode, LED writes, `injectMidiMessage()` /
  `setMessageSink()` for hardware-free tests.
- [Mapping.h](../engine/src/controller/Mapping.h) /
  [Mapping.cpp](../engine/src/controller/Mapping.cpp) — the live mapping
  facade: resolves a pad to a slot, dispatches listeners, hot-reloads on file
  change and owns the polling watcher.
- [MappingModel.h](../engine/src/controller/MappingModel.h) /
  [MappingModel.cpp](../engine/src/controller/MappingModel.cpp) — the
  `SetMapping` document model (families, patches, macros, pads, encoders,
  banks and scenes) and its lookup helpers.
- [MappingYaml.h](../engine/src/controller/MappingYaml.h) /
  [MappingYaml.cpp](../engine/src/controller/MappingYaml.cpp) — the small YAML
  reader and the public `parseSetMappingYaml()` / `loadSetMappingFile()`
  entry points. [MappingYamlInternal.h](../engine/src/controller/MappingYamlInternal.h)
  is the parse-tree contract shared with the validator.
- [MappingValidator.cpp](../engine/src/controller/MappingValidator.cpp) —
  schema and bounds validation of a parsed document into `SetMapping`.
- [PadState.h](../engine/src/controller/PadState.h) — the shared LED state
  enum.

### 3.6 `engine/src/diag/`

- [LogRing.h](../engine/src/diag/LogRing.h) / [LogRing.cpp](../engine/src/diag/LogRing.cpp)
  — fixed-size, trivially-copyable structured log ring and the JUCE logger
  sink.
- [CrashDump.h](../engine/src/diag/CrashDump.h) / [CrashDump.cpp](../engine/src/diag/CrashDump.cpp)
  — async-signal-safe fatal-signal dump, then re-raise.
- [Diagnostics.h](../engine/src/diag/Diagnostics.h) /
  [Diagnostics.cpp](../engine/src/diag/Diagnostics.cpp) — the `--diagnostics`
  report: build identity, device, timing/xruns, plugin inventory, log tail.

### 3.7 `engine/src/dsp/`

Leaf DSP blocks with no engine dependency:

- [Eq.h](../engine/src/dsp/Eq.h) / [Eq.cpp](../engine/src/dsp/Eq.cpp) — 3-band
  low-shelf / bell / high-shelf.
- [Compressor.h](../engine/src/dsp/Compressor.h) /
  [Compressor.cpp](../engine/src/dsp/Compressor.cpp) — feed-forward peak
  compressor.
- [Delay.h](../engine/src/dsp/Delay.h) / [Delay.cpp](../engine/src/dsp/Delay.cpp)
  — tempo-syncable delay.
- [Reverb.h](../engine/src/dsp/Reverb.h) / [Reverb.cpp](../engine/src/dsp/Reverb.cpp)
  — Freeverb-style reverb.

### 3.8 `engine/src/instruments/`

- [Instrument.h](../engine/src/instruments/Instrument.h) /
  [Instrument.cpp](../engine/src/instruments/Instrument.cpp) — the canonical
  `Instrument` interface from the locked decisions (plugin, audio input,
  sampler). It is narrower in the graph: `AudioEngine::SlotSource` is the
  processing seam. The concrete implementations
  (`PluginInstrument`, `SamplerInstrument`, `AudioInputInstrument`, and the
  session-local `InitToneSource`) implement both interfaces directly; the graph
  only sees `SlotSource`.
- [AudioInputInstrument.h](../engine/src/instruments/AudioInputInstrument.h) /
  [AudioInputInstrument.cpp](../engine/src/instruments/AudioInputInstrument.cpp)
  — one interface channel as a source; direct monitoring bypasses the FX
  chain.
- [PluginFx.h](../engine/src/instruments/PluginFx.h) /
  [PluginFx.cpp](../engine/src/instruments/PluginFx.cpp) — a hosted VST3
  wrapped as an FX-chain element, with its own editor and state forwarding.

`PluginInstrument` lives in `plugins/` because it needs the plugin-hosting and
editor infrastructure. `SamplerInstrument` lives in `sampler/` because it needs
the kit and sample loaders. Both implement `Instrument` and `SlotSource`.

### 3.9 `engine/src/plugins/`

- [PluginInstrument.h](../engine/src/plugins/PluginInstrument.h) /
  [PluginInstrument.cpp](../engine/src/plugins/PluginInstrument.cpp) — hosted
  VST3 as an `Instrument`: load, MIDI, process, latency, state round-trip and
  editor. The sandbox is a separate `Instrument` implementation, not a mode of
  this class.
- [PluginInventory.h](../engine/src/plugins/PluginInventory.h) /
  [PluginInventory.cpp](../engine/src/plugins/PluginInventory.cpp) — the
  read-only installed-plugin inventory behind `/plugins`; search, rescan and
  snapshot.
- [Scanner.h](../engine/src/plugins/Scanner.h) / [Scanner.cpp](../engine/src/plugins/Scanner.cpp)
  and [ScannerProcess.h](../engine/src/plugins/ScannerProcess.h) /
  [ScannerProcess.cpp](../engine/src/plugins/ScannerProcess.cpp) — out-of-process
  VST3 scanning; one `ihub-scanner` child per bundle, timeouts and failures.
- [PluginCache.h](../engine/src/plugins/PluginCache.h) /
  [PluginCache.cpp](../engine/src/plugins/PluginCache.cpp) — per-bundle scan
  cache so an unchanged directory does not rescan.
- [PluginWindow.h](../engine/src/plugins/PluginWindow.h) /
  [PluginWindow.cpp](../engine/src/plugins/PluginWindow.cpp) — application-owned
  native editor window. `onBeforeClose` is the capture point for patch state.
- [SandboxHost.h](../engine/src/plugins/SandboxHost.h) /
  [SandboxHost.cpp](../engine/src/plugins/SandboxHost.cpp) and
  [SandboxClient.h](../engine/src/plugins/SandboxClient.h) /
  [SandboxClient.cpp](../engine/src/plugins/SandboxClient.cpp) — out-of-process
  plugin hosting over a shared-memory file mapping; a crash or hang kills only
  `ihub-sandbox`. It is implemented and tested, but the live `Session` selects
  `PluginInstrument` today; see §8.
- [scanner_main.cpp](../engine/src/plugins/scanner_main.cpp) and
  [sandbox_main.cpp](../engine/src/plugins/sandbox_main.cpp) — entry points for
  the two helper executables.
- [ProcessLimits.h](../engine/src/plugins/ProcessLimits.h) — caps an absurd
  `RLIMIT_NOFILE` before plugin loading.

### 3.10 `engine/src/project/`

The `.ihub` set container and its migration. It is a library layer above the
engine and is currently exercised by tests, not by the app (see §8).

- [SetFile.h](../engine/src/project/SetFile.h) / [SetFile.cpp](../engine/src/project/SetFile.cpp)
  — the set document model and validation.
- [Serializer.h](../engine/src/project/Serializer.h) /
  [Serializer.cpp](../engine/src/project/Serializer.cpp) — reads/writes a ZIP
  with `set.json`, `mapping.yaml` and `plugin-state/<uuid>.bin`; atomic save,
  bounded all-or-nothing load. The ZIP entry helpers, `set.json` manifest,
  slot/patch documents and plugin-state blobs live under
  [project/serializer/](../engine/src/project/serializer/); `Serializer.cpp`
  keeps validation and archive orchestration.
- [Migrations.h](../engine/src/project/Migrations.h) /
  [Migrations.cpp](../engine/src/project/Migrations.cpp) — schema upgrades
  (1 → 2 and 2 → 3); never rewrites a document in an old format.

The live per-patch plugin-state cache is `session/PluginStateStore`, not a
second binder in this directory.

### 3.11 `engine/src/sampler/`

- [Kit.h](../engine/src/sampler/Kit.h) / [Kit.cpp](../engine/src/sampler/Kit.cpp)
  — the JSON kit model: velocity layers, round-robin, one-shot/gate, choke
  groups, MIDI note range.
- [KitLoader.h](../engine/src/sampler/KitLoader.h) /
  [KitLoader.cpp](../engine/src/sampler/KitLoader.cpp) — parses and validates a
  kit document.
- [SampleLoader.h](../engine/src/sampler/SampleLoader.h) /
  [SampleLoader.cpp](../engine/src/sampler/SampleLoader.cpp) — WAV/AIFF/FLAC
  decode, resample to the engine rate, RT-safe hand-off.
- [SamplerInstrument.h](../engine/src/sampler/SamplerInstrument.h) /
  [SamplerInstrument.cpp](../engine/src/sampler/SamplerInstrument.cpp) — fixed
  64-voice polyphonic sampler over a loaded kit.

### 3.12 Outside the engine

| Path | Responsibility |
|---|---|
| [ui/src](../ui/src) | React shell: Grid, Instrument, Plugins, Audio, `/remote`. |
| [ui/src/api/client.ts](../ui/src/api/client.ts) | OpenAPI-typed API client; the only place that knows HTTP paths. |
| [ui/src/state/store.ts](../ui/src/state/store.ts) | Polling store, optimistic arm/mix edits, reconnect/backoff. |
| [shared/api/openapi.yaml](../shared/api/openapi.yaml) | HTTP contract and generated TypeScript schema. |
| [shared/set.yaml](../shared/set.yaml) | The shipped 8 × 8 Launchpad set mapping. |
| [shared/schema/set.schema.json](../shared/schema/set.schema.json) | Set mapping schema. |
| [shared/schema/kit.schema.json](../shared/schema/kit.schema.json) | Sampler kit schema. |
| [tests](../tests) | ctest targets, including the pad/click/LED acceptance in `tests/e2e/`. |
| [scripts](../scripts) | `build.sh`, `test.sh`, `dev.sh`, `bootstrap.sh`. |
| [plan](../plan) | Decisions, roadmap, testing recipes, blockers, autopilot. |
| `docs/` | This map, the Launchpad reference and new-machine setup. |

---

## 4. Dependency direction

### 4.1 The compile-time picture

Read this table from the top (leaves) to the bottom (composition root). "May
include" is what the directory is allowed to depend on; anything not listed in
the right-hand column is a layering mistake.

| Layer | May include | Must not include |
|---|---|---|
| `dsp/` | JUCE audio primitives | any engine directory |
| `controller/` | JUCE MIDI, `set.yaml` | `audio/`, `api/`, `Session` |
| `Bridge/` | JUCE GUI/web view | engine business logic |
| `audio/` | `dsp/`, JUCE | `Session`, `api/` (except the `AudioSettings` wart), `sampler/`, `plugins/`, `controller/` |
| `instruments/` | `audio/Slot.h`, `audio/FxChain.h` | `api/`, `Session`, `Main` |
| `sampler/` | `audio/`, `instruments/` | `api/`, `Session`, `Main` |
| `plugins/` | `audio/`, `instruments/` | `api/`, `Session`, `Main` |
| `project/` | `audio/`, `plugins/` | `api/`, `Session` (today) |
| `diag/` | `audio/DeviceManager`, `audio/TimingMonitor`, JUCE | `api/`, `Session` |
| `api/` | `controller/` (pad test only), `plugins/` (inventory only), `Session` (bridge only) | `Main` |
| `Session` | `audio/`, `sampler/`, `plugins/`, `instruments/` | `api/`, `Main`, `controller/` |
| `Main.cpp` | everything | nothing (it is the root) |

In one line: `Main` → {`Api`, `Session`, `Controller`, `Bridge`, `Diag`} →
{`audio`, `sampler`, `plugins`, `instruments`} → `dsp`; `project/` sits beside
`api/` as a persistence library, and `controller/` and `Bridge/` are leaves.

The practical rules:

1. **`dsp/` is a leaf.** It may use JUCE audio primitives but no engine type.
2. **`audio/` is the real-time core.** It may use `dsp/` (through `FxRack`),
   but it must not include `api/`, `Main`, `Session` or the UI. The
   `AudioSettings` bridge is the single existing exception (§4.3).
3. **`instruments/`, `sampler/` and `plugins/` implement the audio seams.**
   They include `audio/Slot.h` and/or `instruments/Instrument.h`; the audio
   core never includes them. This is what keeps the graph independent of how a
   sound is made.
4. **`project/` is above the engine.** It includes `audio/` and `plugins/` to
   bind a saved set into the graph, but no engine file includes `project/`.
5. **`api/` is above the session.** Only `SessionBridge.cpp` includes
   `Session.h`; `Server.h` and `Routes.h` speak in plain structs and
   `std::function` handlers. That is deliberate: the HTTP surface is testable
   without an audio device.
6. **`controller/` is independent.** It knows MIDI and `set.yaml`, not the
   engine. `Main` connects its listener to `StateStore`; the driver never calls
   `Session`.
7. **`Main.cpp` is the only composition root.** It creates the session, the
   store/server, the controller and the window, then installs the lambdas that
   connect them. If you cannot find where a subsystem is constructed, look in
   `Main`.

### 4.2 Runtime wiring vs. compile-time includes

The API does not hold a `Session&` in its server class. `Api::attachSession()`
installs `std::function` handlers on `StateStore`; `Routes` invokes the store,
the store invokes the handler, the handler calls `Session`. This is why the
dependency arrow in the code (`SessionBridge.cpp` → `Session`) does not make
`Server.cpp` depend on the session.

The controller follows the same pattern: `Mapping::Listeners::armSlot` is a
`std::function` installed by `Main`; the mapping layer has no pointer to audio.

### 4.3 Known layering exceptions

These are real today and should not be copied into new code:

- `audio/AudioSettings.h` includes `api/Server.h` for
  `Api::AudioStateSnapshot`, `AudioSettingsPatch` and `AudioMeasurementState`.
  That is an engine → API dependency in the wrong direction, limited to the
  Audio page bridge. A future split can move those DTOs to a neutral header
  (for example `audio/AudioSettingsTypes.h`) or put the bridge in `api/`.
- `audio/Instrument.h` is a one-line compatibility shim that includes
  `instruments/Instrument.h`. New code should include the canonical header.
- `api/MidiBridge.cpp` includes `controller/LaunchpadDriver.h` for the pad
  test. It is a test/inspection seam, not an audio dependency.

---

## 5. The four call paths

Each path is written as the actual sequence of function calls. File links point
at the translation unit; symbol names are stable, line numbers are not.

### 5.1 Launchpad pad → arm → audio

**Preconditions:** `Main::startSession()` has built the session and armed slot
0; `Main::startController()` has loaded `shared/set.yaml`, created the driver
and connected its listeners.

**Step 1 — MIDI decode (controller MIDI thread).**

`LaunchpadDriver::handleIncomingMidiMessage()`
([LaunchpadDriver.cpp](../engine/src/controller/LaunchpadDriver.cpp)) receives a
note on/off from the `Launchpad Mini MK3 MI` port. It calls
`Launchpad::padIndexForNote()` ([LaunchpadProtocol.h](../engine/src/controller/LaunchpadProtocol.h)),
which maps notes 11..88 to pads 0..63, then `LaunchpadDriver::dispatchPad()`
copies the listener list and invokes `DriverListeners::padEvent(pad, pressed)`.
A release is dispatched with `pressed == false`; only a press arms.

`injectMidiMessage()` is the same entry point without a device, used by the
tests.

**Step 2 — pad → slot (controller MIDI thread).**

The driver's `padEvent` listener installed in `Main::startController()` calls
`Mapping::handlePad(pad, pressed)`
([Mapping.cpp](../engine/src/controller/Mapping.cpp)). `handlePad`:

1. copies the current `PadBinding` for the pad from the live `SetMapping`;
2. on press, stores `activePad` and resolves `binding->slot`;
3. calls `Mapping::Listeners::armSlot(slot)` outside the mapping lock;
4. then calls `Mapping::Listeners::padEvent` for the LED refresh.

A pad with no binding is ignored; a release never changes the armed slot.

**Step 3 — mapping listener → state (caller's thread).**

The listener installed by `Main::startController()` is:

```cpp
mappingListeners.armSlot = [this] (int slot)
{
    if (slot >= 0 && apiState != nullptr)
    {
        (void) apiState->setArmedSlot (slot);
        refreshControllerLeds();
    }
};
```

So the controller deliberately goes through the same `StateStore` as HTTP. If
the control server object does not exist (early bind failure), a pad does not
arm. That coupling is intentional: the store is the single writer of the UI
mirror, and `Session` is the single writer of the audio graph.

**Step 4 — store → session (`SessionBridge` arm handler).**

`StateStore::setArmedSlot()` ([StateStore.cpp](../engine/src/api/StateStore.cpp))
validates the slot, updates `armedSlot` under the store mutex, copies the
handlers, then calls the arm handler outside the lock. The handler was
installed by `Api::attachSession()` in
[SessionBridge.cpp](../engine/src/api/SessionBridge.cpp):

```cpp
state.setArmHandler ([&session] (int slot)
{
    const int previous = session.getArmedSlot();

    if (previous >= 0 && previous != slot)
        (void) session.capturePluginState (previous);

    session.armSlot (slot);
});
```

The outgoing patch is captured before the switch, so the controller and HTTP
paths have the same per-patch state guarantee.

**Step 5 — `Session::armSlot()` (control thread).**

`Session::armSlot()` ([Session.cpp](../engine/src/Session.cpp)):

1. rejects an invalid slot and handles `slot < 0` as a disarm;
2. captures the previously active plugin patch, if any;
3. closes every editor that is not bound to the new slot
   (`EditorHost::closeEditorsExcept(slot)`); each close captures its patch
   through `PluginWindow::onBeforeClose`;
4. takes `armMutex`, publishes `pendingArmSlot = slot` and
   `requestedSlot = slot`;
5. if `warmPool->isWarm(slot)`, calls `warmPool->acquire(slot)` and, on
   success, `finishArmLocked(slot, instance)`;
6. otherwise drops the pin on the older previous patch, calls
   `warmPool->setActiveSlot(slot)` and `warmPool->preloadAll()`.
   **The old patch keeps playing.** The switcher has not been asked to change
   yet; `requestedSlot` makes the API/UI report the target, and
   `pendingArmSlot` makes `getWarmState(slot)` report `warming`.

**Step 6 — warm-pool load (pool worker thread), only for a cold slot.**

`WarmPool::run()` ([WarmPoolWorker.cpp](../engine/src/audio/warm/WarmPoolWorker.cpp)) pops the
queued slot. If the live count is at the limit (`1 + K`, default 3), it calls
`evictOneLocked()` to retire the least-recently-used unprotected instance;
destruction is posted to the message thread. It then calls
`Session::createPooledSource()` through the factory installed in
`Session::build()` ([Session.cpp](../engine/src/Session.cpp)):

- `empty` → `SilentSource`
- `initTone` → `InitToneSource`
- `sampler` → `PooledSamplerSource` over `SamplerInstrument`
- `plugin` → `PluginInstrument`, with `applyStoredPluginState(..., force=true)`
  before publication
- `audioInput` → `AudioInputInstrument`

The pool prepares the created instance at the session format, moves it into the
slot, and calls `warmCallback(slot)` on the pool thread.

**Step 7 — hand-off to the graph (pool worker thread).**

`Session::warmCallback()` takes `armMutex`, calls `warmPool->acquire(slot)` to
pin the instance, calls `bindVoice(slot, instance)` and, if
`pendingArmSlot == slot`, calls `finishArmLocked()`; otherwise it releases the
pin (the instance was warmed as a neighbour, not as the arm target).

`bindVoice()` is the only data hand-off the audio thread sees: it stores the
`SlotSource*` into `Voice::delegate` with a release store, after publishing the
latency/input/bypass flags.

**Step 8 — commit the switch (control thread).**

`Session::finishArmLocked()`:

1. calls `applyStoredPluginState(slot)` so an instance whose sidecar revision
   moved is brought up to date;
2. calls `bindVoice(slot, instance)` again;
3. calls `switcher->armSlot(slot)` — **one lock-free atomic store** into
   `SlotSwitcher::pendingSlot`;
4. sets `activeSlot = slot`, clears `pendingArmSlot`, and maintains the
   active/previous pins (`warmPool->release` for the patch that is no longer
   pinned).

**Step 9 — audio thread applies the arm.**

On the next callback, `SlotSwitcher::process()` sees
`requested != consumedSlot`, calls `beginTransition(slot)`, and computes each
slot's `startGain` / `targetGain`. The incoming slot starts its sine leg, the
outgoing slot starts its cosine leg. Every non-idle slot then renders
(`Slot::processUnfaded` → `Voice::process` → instrument `process` → `FxChain`
→ slot gain), and `SlotSwitcher` multiplies each source by `gainAt()` per
sample before summing. After 3 ms the fade is complete: the incoming state
becomes `active` and the outgoing becomes `idle`. The outgoing instance stays
pinned as the "previous" patch until the next arm releases it; only then does
it become eligible for LRU eviction.

**Step 10 — LED mirror.**

Two things repaint the surface:

- the `armSlot` / `padEvent` listeners call `Main::refreshControllerLeds()`
  immediately;
- the normal 100 ms `Timer` in `Main` calls it again, so an arm that arrived
  over HTTP (not the controller) is reflected within one poll.

`refreshControllerLeds()` takes `apiState->snapshot()`, then for every pad
calls `LaunchpadDriver::setPatchLed(pad, colorIndex, armed, warming, error)`.
That resolves to a palette velocity and LED mode and sends it through
`sendPadVelocity()`; unchanged LEDs are suppressed by an atomic compare.

**Failure modes on this path:**

- No `set.yaml` → no mapping; pads do nothing.
- Unbound pad → `handlePad` returns false; no arm.
- Cold slot whose factory fails → `WarmPool` stores `lastError` / `failedId`
  and will not retry that request id; the previous patch remains audible. The
  current `SessionWarmState` enum has no `error` member, so `/state` keeps
  reporting `warming` while `pendingArmSlot` is set; the UI/LED error hook in
  `refreshControllerLeds()` is not reachable from `/state` today.
- API server object absent → the mapping listener cannot arm.
- The audio device is stopped → the switcher state is published but no block
  renders until the device callback runs.

### 5.2 UI click → `/arm` → session

**Step 1 — click (browser/web view).**

`GridView` calls `onArm(slot)`; `App` passes `openPatch`, which calls
`armAndOpenInstrument(slot, arm, navigate)`
([App.tsx](../ui/src/App.tsx)). That function does exactly two things:

```ts
arm (slot);                         // store.armSlot(slot)
navigate (instrumentPathForSlot (slot)); // /instrument?slot=N
```

The navigation is optimistic; the audio request is asynchronous.

**Step 2 — store (browser/web view).**

`AppStore::armSlot()` ([store.ts](../ui/src/state/store.ts)):

1. rejects a slot beyond the last known slot list;
2. increments `armSequence` and `armInFlight`;
3. applies `withArmedSlot(previous, slot)` to the snapshot immediately, so the
   grid highlights before the request leaves;
4. calls `api.armSlot(slot)`.

The API client ([client.ts](../ui/src/api/client.ts)) sends
`POST /arm` with `{"slot": N}` through the generated OpenAPI types. The
request carries `X-Instrument-Hub-Token` (or bearer) when a token is set.

**Step 3 — HTTP server.**

`cpp-httplib` accepts the request on `127.0.0.1:8080` (or the configured
address/port). `Server::Impl` applies CORS, checks auth (loopback management
endpoints stay loopback-only) and dispatches to the route registered in
[Routes.cpp](../engine/src/api/Routes.cpp). The `/arm` handler:

1. parses the JSON object;
2. accepts `slot` or `armedSlot`, a number or `null`;
3. validates the integer range;
4. calls `context.state.setArmedSlot(slot)`;
5. responds `200` with `stateToVar(context.state.snapshot())`, or `400` for a
   bad body/range.

**Step 4 — store → session.**

`StateStore::setArmedSlot()` runs the arm handler installed by
`Api::attachSession()`. That is exactly Step 4 of §5.1: capture the outgoing
patch, then `Session::armSlot(slot)`. The HTTP worker thread is a control
thread; cold loading is allowed there, but never happens on the audio thread.

**Step 5 — response and reconciliation.**

The arm response is the full state document. `AppStore::armSlot()` accepts
it only if `sequence === this.armSequence` (a stale response cannot overwrite a
newer click); it sets `phase: "ready"` and clears the error. On a failed
request it restores the pre-click snapshot. `armInFlight` keeps the optimistic
highlight alive while a 2 s poll may land with the old confirmed value.

**Step 6 — the Instrument page.**

The route already changed to `/instrument?slot=N`. `App` resolves
`instrumentSlotForRoute()` and `patchForInstrumentSlot()` from the family tree
in `/state`, then mounts `InstrumentPage`. On arrival it calls
`store.loadFxChain(slot)` and, if needed, `store.loadPlugins()`. The page shows
the live warm state and latency from the next poll.

**Step 7 — meters and other clients.**

- The Grid/Instrument meters use the `/meters` WebSocket (30 Hz); the server's
  meter thread calls the `MeterProvider` installed in `SessionBridge`, which
  reads `Session::getSlotMeterPeak()` and `getMasterMeterPeak()` from
  audio-thread atomics.
- `StateStore::changeHandler` calls
  `StateBroadcast::publishStateChange("arm", slot)`, which wakes `/events`
  clients with a coalesced state frame. The shipped React store polls
  `/state` every 2 s and does not currently open `/events`; the socket exists
  for clients that want push updates.
- The Launchpad follows within 100 ms through `refreshControllerLeds()`.

**Step 8 — UI-side state shape.**

`/state` is a merged document: the store's static/family data plus a live
`SlotRuntimeProvider` from the session (`armed`, `gain`, `state`, `warmState`,
`warm`, `editorOpen`). The family tree is installed by
`SessionBridge.cpp` at attach time from `Session::getSlotConfigs()`.

### 5.3 Patch edit → plugin state capture

"Patch edit" here means a **sound edit inside a hosted plugin**. Mix, colour,
source and insert edits are separate paths; they are summarised at the end of
this section so the distinction is explicit.

**The sidecar.** Each patch has one opaque blob, `slot-<n>.bin`, in
`set.plugin-state/` next to `set.yaml`. The file is a header (`IHPS`, version
1), a JSON metadata block (slot, revision, plugin path, plugin id, plugin
name), a blob length, then the plugin's `getStateInformation()` bytes. Writes
go through `replaceWithData()` (a sibling temporary file and a rename), with a
64 MiB blob cap and a 64 KiB metadata cap. The blob is never inlined in JSON.

**A. Explicit save (the T132 path).**

1. The Instrument page's "Save sound to patch" button calls
   `onCaptureState(slot)`; `App` calls `store.savePatchSound(slot)`
   ([store.ts](../ui/src/state/store.ts)).
2. The store sets `captureBusy`, then `api.capturePatchState(slot)`
   ([client.ts](../ui/src/api/client.ts)) sends
   `POST /patches/{slot}/state/capture`.
3. The route handler in [Routes.cpp](../engine/src/api/Routes.cpp) validates
   the slot and calls `StateStore::capturePatchState()`.
4. `StateStore` invokes the handler installed by
   [SessionBridge.cpp](../engine/src/api/SessionBridge.cpp), which calls
   `Session::capturePluginState(slot)` and fills `PatchStateCapture` with the
   slot, `captured == true` and `getPluginStateSize(slot)`.
5. `Session::capturePluginState()` ([Session.cpp](../engine/src/Session.cpp)):
   - reads the slot's source descriptor under `sourceMutex`; the slot must be
     `SessionSlotKind::plugin`;
   - under `pluginStateMutex`, requires a registered live
     `PluginInstrument*` for the slot;
   - calls `PluginInstrument::getStateInformation()` into a `juce::MemoryBlock`;
   - if the blob is non-empty and differs from the stored one, moves it into
     `StoredPluginState::data`, increments `revision`, and stores the plugin
     path/id/name;
   - sets `appliedRevision = revision` because the resident instance produced
     this blob;
   - if the blob changed and a sidecar directory is set, calls
     `writePluginState(slot)`.
6. `writePluginState()` copies the blob and metadata under the lock, then calls
   `writePluginStateFile()` for `set.plugin-state/slot-<n>.bin`. The file is
   replaced atomically.
7. The route responds `{"slot": N, "captured": true, "sizeBytes": ...}`. The
   store records `captureSlot`/`capture`, and `InstrumentPage` shows the
   confirmation (for example "Saved to patch · 4.0 kB").

**B. Automatic capture — same function, four triggers.**

- **Switch away.** The `SessionBridge` arm handler calls
  `capturePluginState(previous)` before `Session::armSlot()`. `Session::armSlot`
  also captures the previously active slot at its top.
- **Editor close.** `PluginWindow::onBeforeClose` is installed by
  `Session::EditorHost` as `capturePluginState(slot)`. Both the explicit
  `DELETE /patches/{slot}/editor` and the automatic close on arm reach it.
- **Source change.** `Session::setSlotSource()` captures the outgoing patch
  before retiring the instance; if the new source is a different plugin (or a
  non-plugin), it clears that slot's old blob.
- **Warm-pool eviction / destruction.** `Session::build()` installs the pool's
  destroy hook. Before the instance is destroyed it calls
  `capturePluginStateForInstance()`, which resolves the slot from the
  registered `pluginInstruments` array and calls `capturePluginState(slot)`;
  it then unregisters the instrument and clears the fixed `Voice`. This runs
  on the message thread, where JUCE expects plugin destruction.

Capturing unchanged state is a no-op (`state.data != data` is checked), so the
repeated automatic calls are cheap.

**C. Restore.**

- At startup, `Main::startSession()` sets the sidecar directory before
  `Session::build()`, so `Session::setPluginStateDirectory()` reads every
  `slot-<n>.bin` before the first warm-pool instance is created.
- A fresh `PluginInstrument` in `Session::createPooledSource()` gets
  `applyStoredPluginState(..., force=true)`: a new instance must be restored
  even when the slot revision has not moved since the old instance was
  destroyed.
- `Session::finishArmLocked()` calls `applyStoredPluginState(slot)` for an
  already-resident instance whose revision moved while it was loaded.
- `applyStoredPluginState()` refuses a blob whose stored `path`/`pluginId`
  conflicts with the currently assigned plugin (`pluginStateMatches()`),
  restores outside `pluginStateMutex` (VST3 can take the message-manager lock),
  and marks `appliedRevision` even when the plugin rejects the blob, so a bad
  blob does not block every arm.

**D. The other patch edits (no plugin-state capture).**

| Edit | HTTP | Store → session | What changes |
|---|---|---|---|
| Volume/pan/mute/solo/colour | `PATCH /patches/{slot}` | `StateStore::updatePatch()` → `setPatchHandler` → `Session::setSlotGain()` | The store resolves mute/solo to one effective gain; the slot's atomic gain target changes. Colour is UI/state only. |
| Instrument source | `POST /patches/{slot}/source` | `StateStore::applyPatchSource()` → `Session::setSlotSource()` | Captures the old sound, closes its editor, changes the warm-pool request id, retires the old instance, persists `set.sources.json` through `SourceAssignedHook`. |
| Insert chain | `GET/POST/PUT/PATCH/DELETE /patches/{slot}/fx` | `StateStore` FX handlers → `Session::addSlotFx()` / `replaceSlotFx()` / `updateSlotFx()` / `removeSlotFx()` / `reorderSlotFx()` / `clearSlotFx()` | `fxMutex` + `Slot::setFxChainEditing(true)`, wait for `renderEpoch` to advance, rebuild `FxRack`, refresh latency, publish it. No plugin-instrument state capture. |
| Native editor open/close | `POST/DELETE /patches/{slot}/editor` | `Session::openEditor()` / `closeEditor()` → `EditorHost` | Opening is explicit only; closing captures the patch state. |

### 5.4 The audio callback and the warm pool

**The callback.** `Session::audioDeviceIOCallbackWithContext()`
([Session.cpp](../engine/src/Session.cpp)) is the device's real-time entry
point. The offline/test path is `Session::render()`; both call
`renderInternal()`, so an offline render cannot diverge from the device path.

Sequence for one block:

```
audio device callback
  RtGuard::Scope                         (allocation/lock/log detector)
  TimingMonitor::beginCallback()
  for every slot: Slot::feedDeviceInput  (only sources that accept input keep it)
  blockMidi.clear()
  renderInternal(samples, blockMidi):
      drainMidi(blockMidi)               (AbstractFifo, MIDI thread -> audio)
      Bus::beginBlock(samples)
      SlotSwitcher::process(samples, midi):
          read pendingSlot (acquire)
          if changed: beginTransition()  (start/target gains, warm/fade states)
          for each non-idle slot:
              Slot::processUnfaded()
                  Slot::renderSource()
                      Voice::process()   (acquire delegate, then)
                          instrument->process()
                      FxChain::process() (unless bypassed / chain being edited)
                  Slot::applyGains()
              sum source * gainAt(state, phase) into the switcher mix buffer
          advance fadePosition; finish fading/warming states at 1.0
      Bus::add(switcher output)
      Bus::finish(renderBuffer)          (master limiter, copy to renderBuffer)
      Transport::process(samples)
      updateMeters(samples)              (slot peaks, master peak, decay)
      renderEpoch.fetch_add(1)           (FX edit hand-off)
  copyToOutput(device buffers)
  TimingMonitor::endCallback()
```

The audio thread touches only pre-allocated buffers and atomics: `Voice`'s
delegate, `SlotSwitcher::pendingSlot`, slot gain targets, meter atomics, the
MIDI FIFO, `renderEpoch` and the `fxChainEditing` gate. It never calls
`WarmPool`, `StateStore`, `PluginWindow`, `capturePluginState`, `Session::armSlot`
or any allocator.

**The warm pool.** `WarmPool` is what makes "64 pads, a few plugins" possible:

- **Protected set.** The active slot plus the `K` nearest slot indices
  (default `K = 2`), with lower indices breaking ties. The hard live-instance
  limit is `1 + K` (three by default).
- **Requests.** Each slot has a `WarmPool::Request` with a stable `id`. The
  initial id is `slot:kind:path:frequency`; `setSlotSource()` appends
  `:<sourceRevision>`. Changing the id retires the old instance and queues a
  new one. `payload` carries the slot index.
- **Pins.** `acquire()` returns a borrowed instance and increments its pin.
  `finishArmLocked()` pins the active and previous slots; the pool will not
  destroy a pinned instance. `release()` drops the pin; the last unpinned,
  unprotected instance becomes eligible for LRU eviction.
- **Loads.** One background thread creates instances through the session
  factory. A load never runs on the audio thread or the caller's thread.
  `preloadAll()` is non-blocking; it schedules what fits and reports the rest
  as deferred. Deferred requests are retried when a pin or a delete frees
  room.
- **Destruction.** The pool hands instances to the message thread via
  `MessageManager::callAsync` so JUCE's VST3 wrapper can clean up where it
  expects to. The live count includes pending deletes, so the `1 + K` invariant
  holds across the hand-off. `isIdle()` is the "all posted deletes ran" signal.
- **Cold arm.** `Session::armSlot()` returns while the pool loads. The old
  patch stays audible; `getArmedSlot()` already returns the target;
  `getWarmState(target)` returns `warming`; `finishArmLocked()` publishes the
  switch only once the instance is resident. There is no silence gap by design.
- **Warm arm.** A warm instance is a pointer hand-over plus the 3 ms
  crossfade. `Session::measureSwitch()` and `ihub-e2e` compare that fade with
  the ideal equal-power pair; the residual is far below −60 dBFS.
- **Failure.** A factory error is stored on the slot (`lastError`,
  `failedId`); the previous patch remains and the request id is not retried
  automatically. The state never becomes `warm`; `/state` has no `error`
  member today (see §5.1), so the UI/API sees `warming` while the pending arm
  is set. It never sees a partially bound instance.
- **Why no locks on audio:** the only cross-thread values are the atomic
  delegate pointer, the switch request, gains, meters and MIDI FIFO. The
  warm-pool mutex, `armMutex`, `pluginStateMutex` and `fxMutex` are all
  control-thread locks. If you add a new field the audio thread reads, make it
  an atomic or pre-allocated buffer; never add a mutex.

**Test seam:** `SessionConfig::loadDelayMilliseconds` (default 0) makes the
`warming` → `active` transition observable in the headless warm-pool test. It
is not a production mode.

---

## 6. Threads, locks and lock-free hand-offs

### 6.1 Who may call what

| Call | Thread | Notes |
|---|---|---|
| `Session::build()`, `prepare()`, `openDevice()`, `closeDevice()`, `resetGraph()`, `measureSwitch()` | message thread / device start | The stream must be stopped for graph edits. `prepare()` is also called from `audioDeviceAboutToStart()` before the first block. |
| `Session::armSlot()`, `setSlotSource()`, FX edit methods, `capturePluginState()`, `openEditor()`, `closeEditor()` | control thread (HTTP, controller, message) | May allocate, take control mutexes and schedule pool loads; never audio. `armSlot` takes `armMutex` and may call `WarmPool`. |
| `Session::render()`, `audioDeviceIOCallbackWithContext()`, `renderInternal()` | audio thread | Real-time safe. `render()` is the offline/acceptance entry. |
| `Session::pushMidi()` | MIDI input thread | Fixed-capacity SPSC FIFO; no allocation. |
| Lock-free getters (`getArmedSlot`, `getSlotMeterPeak`, `getSlotLatencySamples`, `getLiveInstrumentCount`, …) | any non-audio thread | Atomics or pre-sized arrays; no locks. |
| Pool-backed getters (`getWarmState`, `isSlotWarm`, `getWarmInstanceLimit`, …) | any non-audio thread | Read `WarmPool`, which takes its own mutex. Never call them from `processBlock()`. |
| `WarmPool::acquire/release/setActiveSlot/preloadAll/isWarm` | any non-audio thread | Take the pool mutex; never call from `processBlock()`. |
| `StateStore` setters / `snapshot` | HTTP, controller, message | Store mutex; the providers run after the lock is released. |
| `LaunchpadDriver` methods | MIDI thread / message thread | Listener list has its own mutex; LED writes go to the MIDI output. |

### 6.2 Lock order

- `Session::sourceMutex` → `Session::pluginStateMutex` when both are needed
  (the factory path). `capturePluginState()` reads the source descriptor first,
  releases `sourceMutex`, then takes `pluginStateMutex`.
- `Session::armMutex` guards the arm hand-off and the pins; it may call
  `WarmPool` (which has its own mutex), but it is never taken from the audio
  thread.
- `Session::fxMutex` serialises chain edits. It is not taken by the audio
  thread; the audio thread only reads `Slot::fxChainEditing`.
- `StateStore::mutex` protects the mirror. Handlers are copied out and called
  after the lock is released, so a handler can call back into the store without
  deadlocking.
- `WarmPool::mutex` protects the pool; callbacks (`WarmCallback`,
  `DestroyHook`) are invoked without it.

When in doubt, copy the handler/listener under the lock, release the lock,
then call it. That pattern appears in `StateStore`, `Mapping`,
`LaunchpadDriver`, `WarmPool` and `Session` for the same reason.

### 6.3 Lock-free hand-offs

| Producer | Consumer | Mechanism |
|---|---|---|
| control thread (`finishArmLocked`) | audio (`SlotSwitcher::process`) | `pendingSlot` atomic, release/acquire |
| control thread (`bindVoice`) | audio (`Voice::process`) | `delegate` atomic pointer, release/acquire |
| control thread (`setSlotGain`/store) | audio (`Slot::applyGains`) | `gainTarget` atomic |
| control thread (`FxChain` edit) | audio (`Slot::renderSource`) | `fxChainEditing` atomic gate + `renderEpoch` |
| MIDI thread (`pushMidi`) | audio (`drainMidi`) | `juce::AbstractFifo` SPSC ring |
| audio (`updateMeters`) | HTTP/UI (`/meters`) | per-slot `std::atomic<float>` peaks |
| audio (`renderInternal`) | FX editor | `renderEpoch` atomic counter |
| state mutations | `/events` clients | `StateBroadcast` sequence atomic + coalesced frame |

---

## 7. Oversized files and target decomposition

The threshold used here is **~800 lines**. At T133 the following files exceed
it, except `Session.h`, which is included because it is the paired facade
header for `Session.cpp` and must split with it. The target splits are the ones
already scheduled in [plan/ROADMAP.md](../plan/ROADMAP.md) (T134–T138 and the
engine splits T162–T168).

| File | Lines | What makes it big | Target decomposition |
|---|---:|---|---|
| [Session.cpp](../engine/src/Session.cpp) | 3167 | Composition, arm/cold hand-off, plugin state, editors, FX racks, meters, device, switch measurement, offline render. | **T136:** keep `Session` as a facade. Extract `session/WarmPoolController` (arm, pins, cold hand-off), `session/PatchStateStore` (sidecar, revisions, restore), `session/EditorHost` (native windows), `session/MeterBridge`, `session/TransportController`, `session/FxController`. Target `Session.cpp` < 800. |
| [api/Routes.cpp](../engine/src/api/Routes.cpp) | 2150 | Every resource and every JSON shape in one registration function. | **T135:** split into `api/routes/{state,patches,fx,audio,midi,plugins,transport,pairing}.cpp`, each registering through one helper. Shared JSON/validation stays in `Routes.h`. Target each route module < 600; no endpoint or status change. |
| [Main.cpp](../engine/src/Main.cpp) | 2001 | Application shell plus eight one-shot modes plus snapshot capture, LED poll and default-set loading. | **T137:** extract `app/modes/*` (diagnostics, selftest, snapshot, serve, session smoke, scan, RT strict, latency) and `app/AppShell` (window/web view). Leave an entry point and composition. Target `Main.cpp` < 600; identical CLI behaviour and output. |
| [api/Server.cpp](../engine/src/api/Server.cpp) | 1779 | `StateStore`, the HTTP server, auth, LAN rebind, meter thread and WebSocket loops. | **T135:** split `StateStore` + DTOs into `api/StateStore.{h,cpp}`; leave `Server` with transport/auth/listener/meter/WebSocket. Then move route bodies out. Target `Server.h`/`Server.cpp` each < 700. |
| [api/Server.h](../engine/src/api/Server.h) | 1037 | All state DTOs, `StateStore` and `Server` in one header. | Same as above: DTOs and `StateStore` move to their own header; `Server.h` keeps the server surface. |
| [Session.h](../engine/src/Session.h) | 776 | Public surface for all of the above. | Follows `Session.cpp`: the facade header keeps build/device/render/arm/inspection; collaborators get their own headers in `session/`. |

Two smaller files are worth watching but are **not** on the split list yet:
`plugins/SandboxHost.cpp` (769), `sampler/KitLoader.cpp` (734),
`api/Discovery.cpp` (725), `controller/LaunchpadDriver.cpp` (617) and
`sampler/SamplerInstrument.cpp` (610). They are cohesive; splitting them now
would add files without reducing coupling.

The intended end state after T134–T138 is: no translation unit above ~800
lines, one route module per resource, `Session` a facade, `Main` a shell, and
no superseded controller or unused persistence path (§8).

---

## 8. Superseded, unwired and overlapping code

These are real parts of the tree that a new contributor will meet and should
not mistake for the live path. T138 removed the superseded controller layer
and the unused `.ihub` plugin-state binder; the remaining items are
capabilities that are tested but not selected by the live factory.

- **`plugins/SandboxHost`** — a complete out-of-process `Instrument`, with
  `ihub-sandbox`, a file-mapping protocol and watchdog tests. `Session` always
  constructs `PluginInstrument` today; the sandbox is a tested capability that
  is not selected by the live factory.
- **`instruments/PluginFx`** — a hosted-VST3 FX wrapper, but the live `FxRack`
  uses its own `InsertEffect` adapter. `PluginFx` is compiled and available,
  not selected by the app.
- **`audio/Instrument.h`** — a compatibility include shim; not a second
  interface.
- **`audio/AudioSettings`** — the Audio page's device bridge, with its own
  `DeviceManager`. The live session owns the device that renders audio. Treat
  the two as separate objects until the layering note in §4.3 is resolved.
- **`ui/src/features/remote`** — the `/remote` stage route is part of the same
  React app and uses the same API/store, plus the `/meters` socket for its
  connection banner. It has no separate backend.

---

## 9. Making a change safely

### 9.1 Add an HTTP endpoint

1. Add the path and schema to [shared/api/openapi.yaml](../shared/api/openapi.yaml);
   regenerate the client (`pnpm run generate:api`) so `ui/src/api/schema.d.ts`
   stays in sync.
2. Add the route in [Routes.cpp](../engine/src/api/Routes.cpp) (or the future
   per-resource module). Parse/validate, then call a `StateStore` method.
3. Add the handler field and method to `StateStore`
   ([StateStore.h](../engine/src/api/StateStore.h)), install the live implementation in
   [SessionBridge.cpp](../engine/src/api/SessionBridge.cpp), and keep the
   plain-struct boundary.
4. If the change reaches audio, go through `Session` on a control thread; never
   add a call from the audio thread.
5. Invoke the store's change handler (the `ChangeHandler` installed by
   `StateBroadcast`) so `/events` and polling clients agree, and update the
   affected slot latency if it changed.
6. Add an API test under [tests/api](../tests/api) and, if it changes sound or
   arm behaviour, an e2e assertion.

### 9.2 Add a built-in effect

1. Implement the processor under `engine/src/dsp/` with `prepare`, `process`,
   `reset`, latency and state.
2. Add the type/id to `FxRack` (`FxInsertType`, string parsing, factory,
   parameters, snapshot).
3. Extend the set schema and the FX route validation.
4. Add a DSP test and an FX-chain/API test.
5. Confirm the empty chain remains bit-exact and the new effect is only
   prepared off the audio thread.

### 9.3 Add an instrument source

1. Implement `Instrument` (and the `SlotSource` adapter if needed).
2. Add a `SessionSlotKind` and a branch in `Session::createPooledSource()`.
3. Give the warm-pool request a unique id for the new source.
4. Add the descriptor to `SessionSlotConfig`, the set schema and the API
   source request, plus the UI picker.
5. Test: cold/warm arm, state restore if hosted, and the 1 + K bound.

### 9.4 Add a controller binding

1. Protocol constants and pure decode functions in
   `LaunchpadProtocol` (no MIDI device, no allocation).
2. Driver decode/dispatch in `LaunchpadDriver`.
3. Routing and validation in `Mapping` (`shared/set.yaml`, listener events).
4. Wire the listener in `Main::startController()`; keep the audio effect on
   the control thread.
5. Add a protocol table test, a mapping test and a driver injection test.

### 9.5 Add a UI page

1. Add the feature directory under `ui/src/features/`, with its model, view and
   CSS.
2. Add the route to `ui/src/state/router.ts` and the shell in
   [App.tsx](../ui/src/App.tsx).
3. Use `ui/src/state/store.ts` for API state and polling; do not call
   `fetch` from a component.
4. Add a server-rendered snapshot in `ui/src/testing/snapshot.tsx` and a page
   assertion. A native web-view snapshot is only needed when the page is the
   release shell itself.

### 9.6 Before you call it done

Run the acceptance check for the area and then the whole suite:

```bash
scripts/test.sh                 # ctest with a 120 s per-test timeout
ctest --test-dir build --output-on-failure
```

For audio-path changes also run:

```bash
./build/engine/instrument-hub --session-smoke
./build/engine/instrument-hub --rt-strict
```

`--session-smoke` proves the composed 64-slot graph renders and switches over
the ALSA Loopback. `--rt-strict` aborts on an allocation, lock or log inside
the callback. If either fails, the change is not done, regardless of unit
tests.

The hardware-free recipes for the controller, the loopback and the snapshots
are in [plan/TESTING.md](../plan/TESTING.md). Physical checks that need a human
are tracked in [plan/BLOCKERS.md](../plan/BLOCKERS.md).

---

## 10. Glossary

| Term | Meaning |
|---|---|
| **Slot** | One fixed position in the audio graph. The default set has 64, one per Launchpad pad. A slot is not an instrument; it points at one. |
| **Patch** | The user-facing unit: a slot plus its name, family, colour, mix, source descriptor and insert chain. |
| **Family** | A row of eight patches in `shared/set.yaml`; the top-row buttons select macro banks, not families, on the Launchpad. |
| **Arm** | Make one slot the audible patch. Warm arm = pointer + 3 ms fade; cold arm = warm pool loads first, old patch stays audible. |
| **Warm / cold** | Warm: the instrument instance is resident in the pool. Cold: only a descriptor exists; arming needs a load. |
| **Warm pool** | The bounded cache of live instrument instances (`1 + K`, default 3) and the background thread that fills it. |
| **Guard** | A slot's fixed `Voice` proxy. It holds an atomic pointer to the current `SlotSource` and is never reallocated. |
| **Sidecar** | A file next to `set.yaml` holding data the set document does not inline: `set.plugin-state/slot-<n>.bin`, `set.sources.json`. |
| **Seam** | A narrow interface that keeps layers apart: `AudioEngine::SlotSource`, `ihub::Instrument`, `Mapping::Listeners`, `StateStore` handlers. |
| **Control thread** | Any non-audio thread that may load, allocate or take a mutex: HTTP worker, MIDI thread, pool worker, message thread. |
| **Audio thread** | The device callback. No allocation, locks, logging, file I/O, graph rewiring or editor calls. |

---

## 11. Related documents

- [HANDOVER.md](../HANDOVER.md) — state, gaps, run modes, verification record.
- [plan/DECISIONS.md](../plan/DECISIONS.md) — locked architecture decisions.
- [plan/TESTING.md](../plan/TESTING.md) — hardware-free acceptance recipes.
- [plan/ROADMAP.md](../plan/ROADMAP.md) — T134–T138 decomposition tasks.
- [plan/BLOCKERS.md](../plan/BLOCKERS.md) — the human queue.
- [plan/AUTOPILOT.md](../plan/AUTOPILOT.md) — session/blocker protocol.
- [docs/launchpad-mini-mk3.md](launchpad-mini-mk3.md) — controller reference.
- [docs/new-machine.md](new-machine.md) — one-time host setup.
- [shared/api/openapi.yaml](../shared/api/openapi.yaml) — HTTP contract.
