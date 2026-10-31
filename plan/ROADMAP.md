# Roadmap — task queue

One task = one session. Format is parsed by `autopilot.py`; keep the
`## T0xx ·` headers and the `- key:` lines intact.

Rules for every task: implement, run the acceptance check, write the result
file. If the check fails you are not done.

---

## T000 · Preflight and repo bootstrap
- depends: none
- size: S
- goal: Install dev dependencies, `git init`, commit the plan pack, vendor JUCE 8 as a submodule, add `.gitignore`. Create `AGENTS.md` in the repo root with the guardrails from `AUTOPILOT.md` §5.
- files: `AGENTS.md`, `.gitignore`, `third_party/JUCE/`, `plan/`
- accept: `git -C /home/mkocevar/Projects/instrument-hub log --oneline` shows an initial commit; `third_party/JUCE/CMakeLists.txt` exists; `pkg-config --exists alsa` exits 0.
- note: Needs sudo. If `pkg-config` or `alsa` is missing, write `STATUS: blocked` per `BLOCKERS.md` B001. Do not attempt to install.

## T001 · CMake + JUCE GUI skeleton
- depends: T000
- size: M
- goal: `engine/CMakeLists.txt` using `juce_add_gui_app`, a `Main.cpp` opening an empty 1200x800 window, `CMakePresets.json`, and a build script. Link `juce_audio_devices`, `juce_audio_processors`, `juce_audio_utils`, `juce_dsp`, `juce_gui_extra`.
- files: `engine/CMakeLists.txt`, `engine/src/Main.cpp`, `CMakePresets.json`, `scripts/build.sh`
- accept: `scripts/build.sh` exits 0 and produces a binary; running it under `DISPLAY=:0` opens a window for 3 s without crashing (add a `--selftest` flag that quits after 3 s).

## T002 · React/Vite skeleton + embed
- depends: T001
- size: M
- goal: `ui/` with Vite + React + TS + strict mode. `WebBrowserComponent` loads `http://localhost:5173` in Debug and embedded `BinaryData` in Release. `juce_add_binary_data` wired to `ui/dist`. Add `scripts/dev.sh` to run both.
- files: `ui/`, `engine/src/Bridge/WebView.{h,cpp}`, `engine/CMakeLists.txt`
- accept: Release build shows a React-rendered "Instrument Hub" heading; Debug build reloads on save.

## T003 · ALSA loopback smoke test
- depends: T001
- size: M
- goal: A headless CLI target `ihub-selftest` that opens the ALSA `Loopback` device (card 2, playback side), writes 1 s of 440 Hz sine at 48 kHz / 128 frames, captures from the loopback capture side, and writes `out.wav`.
- files: `tests/selftest/AudioSmoke.cpp`
- accept: WAV is 48000 frames, RMS within 1 dB of the source, and the capture contains no silent gap.

## T004 · CI matrix
- depends: T001
- size: S
- goal: GitHub Actions building Linux, Windows, macOS on push. No signing, no release.
- files: `.github/workflows/build.yml`
- accept: workflow YAML parses (`python3 -c "import yaml,sys;yaml.safe_load(open(...))"`); Linux job mirrors `scripts/build.sh` steps.

---

## T010 · Audio device manager wrapper
- depends: T003
- size: M
- goal: `AudioEngine::DeviceManager` wrapping `juce::AudioDeviceManager`. Enumerate device types, devices, sample rates, buffer sizes; expose input/output channel masks; report `getInputLatencyInSamples()` / `getOutputLatencyInSamples()`. Hot-swap device without restarting the app.
- files: `engine/src/audio/DeviceManager.{h,cpp}`
- accept: a `--list-devices` flag prints the Loopback device with its sample rates and buffer sizes; a unit test switches buffer size 128→256→128 while running with no crash.

## T011 · Engine and fixed slot graph
- depends: T010
- size: L
- goal: `AudioEngine` owning N pre-allocated `Slot` objects wired in parallel to a summing bus, then a master limiter. Only slots with `active` or `fading` get `processBlock()`. No allocation after `prepare()`.
- files: `engine/src/audio/{AudioEngine,Slot,Bus}.{h,cpp}`
- accept: a test renders 2 s with 3 slots, asserts the sum equals a manually mixed reference within -90 dBFS; asserts zero allocations during `processBlock` using a counting allocator.

## T012 · Round-trip latency harness
- depends: T011
- size: M
- goal: Send an impulse through the loopback, capture, cross-correlate, report round-trip in ms and samples. Expose as `--measure-latency` and as an engine method for the UI.
- files: `engine/src/audio/LatencyProbe.{h,cpp}`, `tests/`
- accept: measured round-trip at 128 frames is within ±1 buffer of `bufferSize*2/sr`; running it 5 times returns consistent values.

## T013 · Xrun and timing monitor
- depends: T011
- size: S
- goal: Time each callback, compare against the deadline, count overruns and dropped blocks. Expose counters and a rolling max-callback-time.
- files: `engine/src/audio/TimingMonitor.{h,cpp}`
- accept: a test injects a deliberate 2x deadline stall and the counter increments exactly once per stall.

## T014 · Realtime safety harness
- depends: T011
- size: S
- goal: A debug-only assert layer that fails the test if `processBlock` allocates, locks, or logs. Wire a custom `juce::LeakedObjectDetector`-style guard and a `--rt-strict` flag.
- files: `engine/src/audio/RtGuard.{h,cpp}`, `tests/`
- accept: `--rt-strict` passes on the normal path and aborts on an injected `new` inside the callback.

---

## T020 · Test plugin (our own VST3)
- depends: T011
- size: M
- goal: Build a minimal JUCE VST3 `TestPlugin` (gain + known latency + a sine generator) into `tests/plugins/`. Used by all plugin tests so CI needs no third-party binaries.
- files: `tests/plugins/TestPlugin/`, `tests/plugins/CMakeLists.txt`
- accept: `TestPlugin.vst3` builds and is found by `juce::AudioPluginFormatManager`.

## T021 · Plugin scanner, out of process
- depends: T020
- size: L
- goal: Scan VST3 directories in a separate helper binary so a crashing plugin cannot kill the app. Write results to `KnownPluginList` XML in the user data dir.
- files: `engine/src/plugins/{Scanner,ScannerProcess}.{h,cpp}`, `engine/src/plugins/scanner_main.cpp`
- accept: scanning a directory containing a deliberately crashing fake DLL logs a failure and the app survives; the good `TestPlugin` is still listed.

## T022 · Plugin instance wrapper and state
- depends: T021
- size: L
- goal: `PluginInstrument` implementing `Instrument`, wrapping `juce::AudioPluginInstance`. Load/unload, `prepare`, MIDI in, `getStateInformation`/`setStateInformation` round-trip to a blob.
- files: `engine/src/plugins/PluginInstrument.{h,cpp}`
- accept: instantiate `TestPlugin`, set a non-default parameter, save state, reload into a new instance, assert the parameter matches.

## T023 · Plugin editor embedding
- depends: T022
- size: M
- goal: Open a plugin's native editor in a child window owned by the app; close cleanly on unload.
- files: `engine/src/plugins/PluginWindow.{h,cpp}`
- accept: `TestPlugin`'s editor opens under `DISPLAY=:0`, stays open 2 s, closes without leaking (JUCE leak detector clean).

## T024 · Plugin cache management
- depends: T021
- size: S
- goal: Cache load, merge, dead-man's-pedal list for plugins that crashed at scan time.
- files: `engine/src/plugins/PluginCache.{h,cpp}`
- accept: second scan of the same directory completes in under 100 ms using the cache.

---

## T030 · Instrument interface and audio input
- depends: T011
- size: M
- goal: Define `Instrument` exactly as in `DECISIONS.md`. Implement `AudioInputInstrument` (interface channel → optional gain → out) with its own latency accounting.
- files: `engine/src/instruments/{Instrument,AudioInputInstrument}.{h,cpp}`
- accept: loopback sine injected on an input channel appears on the master bus at unity gain.

## T031 · Slot and FX chain
- depends: T030
- size: L
- goal: `Slot` = instrument → ordered `FxChain` → bus. FX are `juce::AudioProcessor` so built-in and hosted effects share one path. Empty chain is a bit-exact passthrough.
- files: `engine/src/audio/{Slot,FxChain}.{h,cpp}`
- accept: empty chain passthrough is bit-identical; a 2-effect chain applies in order (verified with the TestPlugin).

## T032 · Gain-ramp switching
- depends: T031
- size: L
- goal: `armSlot(id)` performs a 3 ms equal-power crossfade on the audio thread via atomic state, no locks. Slot states: `idle`, `warming`, `active`, `fading`.
- files: `engine/src/audio/SlotSwitcher.{h,cpp}`
- accept: switching between two sine-generating slots produces no sample discontinuity above -60 dBFS.

## T033 · Warm pool and preload
- depends: T032
- size: M
- goal: Keep active + K neighbours instantiated; LRU-evict others on a background thread; `preloadAll()` for a set.
- files: `engine/src/audio/WarmPool.{h,cpp}`
- accept: with 8 slots and K=2, at most 3 plugin instances exist at any time; arming a warm slot completes in under 5 ms.

## T034 · Click/discontinuity test
- depends: T032
- size: M
- goal: Automated test rendering 50 random switches across slots while analysing the master bus for discontinuities (sample-to-sample delta threshold and spectrum splatter).
- files: `tests/audio/SwitchContinuity.cpp`
- accept: 50 switches, zero detections; test fails when the ramp is disabled.

---

## T040 · Launchkey protocol module
- depends: T001
- size: M
- goal: Pure, hardware-free logic: session-mode SysEx bytes, pad note↔index map, LED palette velocity table, encoder CC map, key range. Sourced from the Novation Programmer's Reference. No MIDI I/O in this layer.
- files: `engine/src/controller/LaunchkeyProtocol.{h,cpp}`, `docs/launchkey-mini-mk3.md`
- accept: unit tests: note 36..51 map to pads 0..15; every palette colour produces a distinct velocity byte; SysEx frame length and checksum match the documented example.

## T041 · Virtual Launchkey simulator
- depends: T040
- size: M
- goal: An ALSA-sequencer virtual MIDI port (`Launchkey Mini MK3 SIM`) that can play back scripted event files and record everything the app sends back. Support `--replay fixture.json`.
- files: `tests/midi/LaunchkeySim.cpp`, `tests/midi/fixtures/`
- accept: `ihub-selftest --midi-probe` lists the SIM port; a fixture of 16 pad presses is received in order.

## T042 · Controller driver
- depends: T041
- size: L
- goal: `LaunchkeyDriver` on top of `juce::MidiInput`/`MidiOutput`: device detection, SysEx handshake on connect, pad event dispatch to the mapping, LED feedback with state colours (idle/warming/active/error), key notes forwarded to the armed slot.
- files: `engine/src/controller/LaunchkeyDriver.{h,cpp}`
- accept: against the simulator, connect → handshake bytes match the fixture → pad 0 press arms slot 0 → LED command for pad 0 emitted with the active colour.

## T043 · set.yaml mapping and hot reload
- depends: T042
- size: M
- goal: YAML loader (vendored `yaml-cpp` or a small parser) for families, pads, encoders, macros. Watch the file and apply changes without restarting audio.
- files: `engine/src/controller/Mapping.{h,cpp}`, `shared/schema/set.schema.json`
- accept: editing a pad's instrument in `set.yaml` changes routing within 500 ms with no dropout.

## T044 · Driver integration tests
- depends: T043
- size: M
- goal: End-to-end: simulator presses pads per a fixture, app switches slots, LEDs recorded and compared to a golden file.
- files: `tests/midi/DriverIntegration.cpp`
- accept: golden LED stream matches exactly; test fails if the handshake is removed.

---

## T050 · Control API server
- depends: T011
- size: L
- goal: HTTP + WebSocket on `127.0.0.1:8080`. Endpoints: `GET /state`, `POST /arm`, `GET /settings`, `POST /settings`, `POST /transport`, `GET /meters` (WS). Token auth via header when enabled.
- files: `engine/src/api/{Server,Routes}.{h,cpp}`, `shared/api/openapi.yaml`
- accept: `curl localhost:8080/state` returns valid JSON; a WS client receives at least 10 meter frames per second.

## T051 · React app and typed client
- depends: T050
- size: M
- goal: Vite app with a generated typed client from `shared/api/openapi.yaml`, state store, reconnect logic.
- files: `ui/src/`, `ui/package.json`
- accept: `pnpm build` succeeds; the app renders device state fetched from the API in headless Chrome-less mode (snapshot test).

## T052 · Main grid UI and meters
- depends: T051
- size: L
- goal: 8x8-capable grid (families × patches), armed highlighting, per-slot level meters, loading state. Touch-friendly.
- files: `ui/src/features/grid/`
- accept: snapshot shows 8 families; arming via the API updates the highlighted cell within one frame.

## T053 · Audio settings panel
- depends: T051
- size: M
- goal: Device type, device, sample rate, buffer size, channel mapping, live latency readout (buffer + device + per-chain total), xrun counter, "Measure" button calling the latency probe.
- files: `ui/src/features/audio-settings/`
- accept: changing buffer size in the UI changes it in the engine and the readout updates; Measure returns a plausible number on loopback.

## T054 · Embed and snapshot test
- depends: T052
- size: M
- goal: Serve embedded assets in Release, keep the dev URL in Debug, and add `--snapshot <path>` that renders the UI and writes a PNG.
- files: `engine/src/Bridge/WebView.cpp`, `tests/ui/screenshot.cpp`
- accept: `--snapshot out.png` produces a non-blank 1200x800 PNG in Release build.

---

## T060 · Built-in EQ
- depends: T031
- size: M
- goal: 3-band parametric EQ with `juce::dsp::IIR`, RT-safe coefficient updates.
- files: `engine/src/dsp/Eq.{h,cpp}`
- accept: magnitude response within 0.5 dB of the analytic target at 3 test frequencies.

## T061 · Compressor
- depends: T060
- size: M
- goal: Feed-forward compressor with attack/release/threshold/ratio/makeup, reported lookahead latency.
- files: `engine/src/dsp/Compressor.{h,cpp}`
- accept: 6 dB over threshold yields the expected gain reduction within 1 dB; reported latency matches measured.

## T062 · Delay and reverb
- depends: T061
- size: M
- goal: Tempo-syncable delay (accepts transport BPM/PPQ) and an algorithmic reverb.
- files: `engine/src/dsp/{Delay,Reverb}.{h,cpp}`
- accept: a 1/4-note delay at 120 BPM places the echo at exactly 12000 samples at 48 kHz.

## T063 · VST3 FX slot
- depends: T061
- size: M
- goal: Wrap a hosted VST3 as an FX-chain element, including editor opening and state.
- files: `engine/src/instruments/PluginFx.{h,cpp}`
- accept: `TestPlugin` in FX mode applies its gain and reports its latency into the chain total.

## T064 · Per-chain latency readout
- depends: T063
- size: S
- goal: Sum instrument + FX latency per slot, publish to the API, show in the UI.
- files: `engine/src/audio/Slot.cpp`, `ui/src/features/grid/`
- accept: inserting a 5 ms-latency plugin increases the slot's reported total by exactly 5 ms.

---

## T070 · Audio input slot and direct monitoring
- depends: T030
- size: M
- goal: First-class `AudioInput` slot type with its own latency budget and a direct-monitor path that can bypass heavy effects.
- files: `engine/src/instruments/AudioInputInstrument.cpp`
- accept: input → FX → bus works on loopback; direct-monitor mode bypasses the chain.

## T071 · Transport core
- depends: T011
- size: M
- goal: Sample-accurate transport: BPM, PPQ, playing state, tap tempo, external sync hook.
- files: `engine/src/audio/Transport.{h,cpp}`
- accept: after 10 s at 120 BPM, PPQ is 20.0 ± 1 tick at 48 kHz.

## T072 · MIDI clock out
- depends: T071
- size: M
- goal: 24 PPQN clock + start/stop/continue on a chosen MIDI output, timestamped from the transport.
- files: `engine/src/audio/ClockOut.{h,cpp}`
- accept: simulated 10 s at 120 BPM emits exactly 480 clock messages; jitter under 1 ms.

## T073 · MIDI clock in
- depends: T072
- size: M
- goal: Slave mode: follow incoming clock, smooth estimation, detect stop.
- files: `engine/src/audio/ClockIn.{h,cpp}`
- accept: feeding 120 BPM clock from the simulator locks the transport within 2 s.

## T074 · Tap tempo
- depends: T071
- size: S
- goal: Tap tempo from controller button or API, median-of-last-N.
- files: `engine/src/audio/TapTempo.{h,cpp}`
- accept: taps spaced 500 ms ± 20 ms converge to 120 ± 1 BPM after 4 taps.

---

## T080 · Set file format
- depends: T043
- size: L
- goal: `.ihub` container (ZIP): `set.json`, `plugin-state/<uuid>.bin`, `mapping.yaml`. Save/load the whole session.
- files: `engine/src/project/{SetFile,Serializer}.{h,cpp}`
- accept: build a set with 4 slots and 2 plugin states, save, reload into a fresh engine, assert deep equality.

## T081 · Plugin state chunks
- depends: T022
- size: S
- goal: Per-instance state blobs with UUIDs, version stamp, and safe failure when a plugin is missing.
- files: `engine/src/project/PluginStateStore.{h,cpp}`
- accept: a set referencing a missing plugin loads with a placeholder slot and a warning, no crash.

## T082 · Set migration
- depends: T080
- size: S
- goal: `schemaVersion` field and a migration chain.
- files: `engine/src/project/Migrations.{h,cpp}`
- accept: a v1 fixture loads under v2 with defaults filled in.

---

## T090 · Sample loader
- depends: T011
- size: L
- goal: Decode WAV/AIFF/FLAC via `juce::AudioFormatManager`, resample to engine rate, background load, RT-safe handoff.
- files: `engine/src/sampler/SampleLoader.{h,cpp}`
- accept: loads a 48 kHz and a 44.1 kHz file; output lengths and pitch are correct at 48 kHz.

## T091 · Kit format and velocity layers
- depends: T090
- size: M
- goal: JSON kit definition: pads, velocity layers, round-robin, gain/pan/tune.
- files: `engine/src/sampler/{Kit,KitLoader}.{h,cpp}`, `shared/schema/kit.schema.json`
- accept: a 2-layer kit plays layer 1 below the split velocity and layer 2 above.

## T092 · Pad triggering and choke
- depends: T091
- size: M
- goal: Polyphonic voice pool, one-shot and gate modes, choke groups (hi-hat).
- files: `engine/src/sampler/SamplerInstrument.{h,cpp}`
- accept: 16 simultaneous hits do not clip the pool; a closed hat chokes the open hat within 5 ms.

---

## T100 · Remote route
- depends: T052
- size: M
- goal: `/remote` route: large touch targets, set list, next/prev patch, transport, panic. Responsive.
- files: `ui/src/features/remote/`
- accept: snapshot at 1024x768 shows no horizontal scroll and all controls ≥ 44 px.

## T101 · QR pairing
- depends: T050
- size: M
- goal: LAN mode toggle (off by default), 6-digit pairing code, QR code rendered in the desktop UI, token stored client-side.
- files: `engine/src/api/Pairing.{h,cpp}`, `ui/src/features/pairing/`
- accept: unpaired request gets 401; pairing with the code returns a token; token grants `/state`.

## T102 · mDNS discovery
- depends: T101
- size: S
- goal: Advertise `_instrumenthub._tcp` when LAN mode is on.
- files: `engine/src/api/Discovery.{h,cpp}`
- accept: `avahi-browse -t _instrumenthub._tcp` lists the service (skip gracefully if avahi is absent).

## T103 · Live state mirror
- depends: T100
- size: M
- goal: Hardware pad presses and UI taps both update all connected clients within 100 ms.
- files: `engine/src/api/StateBroadcast.{h,cpp}`
- accept: an event injected by the MIDI simulator appears in a WS client's state within 100 ms.

## T104 · Remote polish
- depends: T100
- size: S
- goal: Optimistic UI, reconnect banner, wake-lock, dark high-contrast theme for stage use.
- files: `ui/src/features/remote/`
- accept: killing the WS connection shows the banner and recovers automatically within 3 s.

---

## T110 · Plugin crash isolation
- depends: T021
- size: L
- goal: Optional out-of-process plugin hosting for instruments, with shared-memory audio transport.
- files: `engine/src/plugins/{SandboxHost,SandboxClient}.{h,cpp}`
- accept: a fake plugin that aborts kills only the sandbox; the app reports the slot as failed and keeps audio running.

## T111 · Diagnostics
- depends: T013
- size: S
- goal: Structured log ring buffer, crash-safe dump, `--diagnostics` report with device, buffer, latency, xruns, loaded plugins.
- files: `engine/src/diag/`
- accept: `--diagnostics` prints a complete report with the loopback device.

## T112 · Packaging
- depends: T004
- size: M
- goal: AppImage (Linux), MSI (Windows), DMG (macOS) CI jobs. Unsigned.
- files: `.github/workflows/release.yml`
- accept: Linux job produces a runnable AppImage artifact.

## T113 · Handover
- depends: T000
- size: S
- goal: `HANDOVER.md`: what works, what is stubbed, every hardware check remaining, how to build and run on each OS.
- files: `HANDOVER.md`
- accept: file exists, lists every open item from `BLOCKERS.md`, and a build command that was actually run.

## T114 · Compose the audio session
- depends: T021, T032, T090, T091, T092
- size: L
- goal: `Main` owns an `AudioEngine` and a `SlotSwitcher`, opens the configured audio device on startup, and loads a default set into the eight slots through the existing instrument/sampler/plugin factories. No graph rewiring at runtime; keep the audio-thread contract. Add a headless `--session-smoke` mode that opens the ALSA loopback, arms a slot, renders audio and reports RMS plus the switching discontinuity.
- files: `engine/src/Session.{h,cpp}`, `engine/src/Main.cpp`, `tests/audio/SessionSmoke.cpp`
- accept: `--session-smoke` exits 0, reports a non-zero RMS for the armed slot, and reports no sample discontinuity above -60 dBFS across a slot switch; `ctest` stays green.

## T115 · Wire the API to the session
- depends: T114, T050, T071
- size: M
- goal: `/arm` calls `SlotSwitcher::armSlot`, `/transport` start/stop/position drives the session, `/meters` reads the real engine bus, and `/state` publishes the actual slot states instead of synthetic values.
- files: `engine/src/api/Server.cpp`, `engine/src/api/Routes.cpp`, `engine/src/Main.cpp`, `tests/api/`
- accept: in a headless session, `POST /arm` 0 then 1 changes the audible slot, `/meters` reports non-zero for the playing slot, and `ctest` stays green.

## T116 · Launchpad 64-pad mapping and control rows
- depends: T043, T114
- size: M
- goal: Map all 64 grid pads to 8 families × 8 patches from `set.yaml` (this supersedes the 16-pad Launchkey tasks T040–T044; the Launchpad protocol and driver already exist). Use the top row (CC 91..98) for macro/page banks in place of the missing encoders, the right column (CC 89..19) for scenes, and drive LEDs with idle/warming/active/error. Keep the driver free of audio knowledge.
- files: `engine/src/controller/Mapping.{h,cpp}`, `engine/src/controller/LaunchpadDriver.{h,cpp}`, `shared/set.yaml`, `tests/controller/`
- accept: unit tests resolve all 64 pads to their bindings; a scripted driver event sequence produces the expected armed slot and LED stream; `ctest` stays green.

## T117 · End-to-end pad-to-audio acceptance
- depends: T115, T116
- size: M
- goal: One headless acceptance test that sends a pad event through the controller mapping into the composed session on the ALSA loopback, verifies the intended slot arms, audio is non-silent, and switching is click-free. Update `HANDOVER.md`, `BLOCKERS.md` and the controller docs to the Launchpad reality.
- files: `tests/e2e/`, `HANDOVER.md`, `plan/BLOCKERS.md`, `docs/`
- accept: `ctest -R ihub-e2e` passes and records the rendered RMS and switch measurement; `HANDOVER.md` no longer describes the app as uncomposed.

## T118 · 64 logical slots and warm pool
- depends: T033, T114
- size: L
- goal: Extend the composed session from 8 to 64 logical slots (one per pad), with only the armed slot plus K neighbours instantiated. Arming a cold patch loads it, reports `warming`, then crossfades in; the previous patch stays warm for instant return. Expose the family/patch tree and per-slot warm state through `/state`. Keep the audio-thread contract and the fixed graph.
- files: `engine/src/Session.{h,cpp}`, `engine/src/audio/WarmPool.{h,cpp}`, `engine/src/Main.cpp`, `engine/src/api/Server.{h,cpp}`, `tests/`
- accept: a headless test configures 64 patches, verifies at most K+1 instruments are instantiated, arms a cold slot, observes warming -> active and non-silent audio, and confirms the memory/instance bound; `ctest` stays green.

## T119 · Per-patch persistence
- depends: T080, T081, T118
- size: M
- goal: Persist every patch in `set.yaml`/`.ihub`: instrument descriptor, volume/pan/mute/solo, `colorIndex` (family default plus patch override) and FX chain. Store plugin state lazily for patches that have been loaded/edited; cold patches are descriptors only. Migrate existing sets.
- files: `engine/src/controller/Mapping.{h,cpp}`, `engine/src/project/Serializer.cpp`, `engine/src/project/SetFile.cpp`, `shared/schema/set.schema.json`, `shared/set.yaml`, `tests/`
- accept: a 64-patch set round-trips; cold patches carry descriptors only; a loaded patch restores its plugin state; older sets migrate; `ctest` stays green.

## T120 · Instrument, mix and colour API
- depends: T115, T118
- size: M
- goal: Publish families -> patches in `/state` (instrument descriptor, `volumeDb`, `pan`, `mute`, `solo`, `colorIndex`, warm state, latency). Add `PATCH /patches/{slot}` for those fields and coalesced parameter-change events over the existing WebSocket.
- files: `engine/src/api/Server.{h,cpp}`, `engine/src/api/Routes.cpp`, `shared/api/openapi.yaml`, `ui/src/api/schema.d.ts`, `tests/api/`
- accept: changing volume or colour updates audio/state and reaches a WebSocket client; `/state` carries the full patch tree; `ctest` stays green.

## T121 · Per-patch FX chain API
- depends: T119, T120
- size: M
- goal: Read and edit each patch's insert chain over the API: add/remove/reorder/bypass and wet/dry for the built-in EQ, compressor, delay, reverb and VST3 effects, with per-chain latency reporting.
- files: `engine/src/api/`, `engine/src/audio/Slot.cpp`, `shared/api/openapi.yaml`, tests
- accept: a chain can be built and reordered through the API and the rendered audio changes accordingly; latency is reported; `ctest` stays green.

## T122 · Plugin inventory API
- depends: T021
- size: M
- goal: Read-only inventory of installed VST3s from the existing scanner: name, vendor, version, category, instrument/FX, path. Add a rescan trigger and search. This is the Plugins page data source; assignment happens per patch, not here.
- files: `engine/src/plugins/Scanner.{h,cpp}`, `engine/src/plugins/PluginCache.{h,cpp}`, `engine/src/api/`, `shared/api/openapi.yaml`, tests
- accept: `/plugins` lists `TestPlugin` and any installed VST3 (including yabridge-wrapped Arturia), a rescan picks up changes, and `ctest` stays green.

## T123 · UI shell, routes and Grid page
- depends: T120
- size: L
- goal: Routed shell for `/grid`, `/instrument`, `/plugins`, `/audio` (plus existing `/remote`). The Grid page renders 8 families x 8 patches with their colours, arms on click and navigates to the Instrument page, and edits per-patch/family colours. The armed cell is bright red; Launchpad LEDs mirror the grid (idle colour, armed red, warming yellow, error flashing).
- files: `ui/src/App.tsx`, `ui/src/features/grid/`, `ui/src/styles.css`, `ui/src/state/`, `engine/src/Main.cpp`, tests
- accept: snapshot shows 64 coloured cells with the armed one red; clicking arms and routes; a colour change updates the cell and the driver LED; routes resolve; `ctest` and the UI snapshot stay green.

## T124 · Instrument page
- depends: T121, T122, T123
- size: L
- goal: The armed instrument's page: name/family/colour, warm state, latency, volume fader, balance, mute/solo, instrument source (change/open editor), and the insert FX chain editor with add/remove/reorder/bypass. The page follows the controller selection without yanking the user off other pages.
- files: `ui/src/features/instrument/`, `ui/src/api/`, tests
- accept: selecting a patch shows its real data; moving the fader changes the rendered level; the chain can be edited; snapshot and `ctest` stay green.

## T125 · Plugins inventory page
- depends: T122, T123
- size: S
- goal: Installed-plugin inventory page with search, scan trigger and metadata; no assignment controls. Show a "used by" hint where a plugin is assigned to a patch.
- files: `ui/src/features/plugins/`, tests
- accept: the page lists the scanned plugins, search filters them, rescan works; snapshot stays green.

## T126 · Audio and MIDI page
- depends: T123
- size: M
- goal: Move the existing audio settings panel onto its routed page and add MIDI inputs with enable checkboxes, Launchpad status/port (MI), Keystation, and a pad test that lights the grid on the device.
- files: `ui/src/features/audio-settings/`, `ui/src/features/midi/`, `engine/src/api/`, tests
- accept: audio settings still apply; MIDI inputs reflect the ALSA devices; the pad test lights the Launchpad; snapshot stays green.

## T127 · Page end-to-end and snapshots
- depends: T123, T124, T125, T126
- size: M
- goal: One acceptance pass over the pages: grid click and Launchpad pad both arm the same patch, audio is non-silent, the LED matches the patch colour with the armed pad red; snapshots for Grid, Instrument, Plugins and Audio pages. Update `HANDOVER.md` and `BLOCKERS.md`.
- files: `tests/e2e/`, `tests/ui/`, `HANDOVER.md`, `plan/BLOCKERS.md`, `docs/`
- accept: `ctest -R 'ihub-e2e|ihub-ui'` passes and records the pad/click/LED/audio evidence; snapshots are produced; handover reflects the pages.

## T128 · Live MIDI into the session
- depends: T114, T126
- size: M
- goal: Forward enabled MIDI inputs (Keystation and any non-Launchpad input) into the audio session through a lock-free SPSC queue. The MIDI thread calls `Session::pushMidi()`; the audio callback drains the queue into the block's `MidiBuffer` without allocation or locks. Launchpad MI notes remain controller events (arm), never instrument notes. Add an `empty` source kind so unassigned patches are silent instead of `init-tone`, and make the shipping `shared/set.yaml` use `empty` (or a real assigned source) while tests keep their own init-tone fixture. The Instrument page source picker must persist the assignment.
- files: `engine/src/Session.{h,cpp}`, `engine/src/api/MidiBridge.{h,cpp}`, `engine/src/Main.cpp`, `shared/set.yaml`, `tests/`
- accept: an injected note on the armed plugin patch renders non-silent audio (and a note-off stops it); unassigned patches are silent; Launchpad pads arm but do not play notes; the e2e test uses a dedicated init-tone fixture instead of the shipping set; `ctest` stays green.

## T129 · Native plugin editor windows
- depends: T124, T128
- size: M
- goal: Wire the existing `plugins::PluginWindow` to the composed session. `Session::openEditor(slot)` resolves the patch's hosted `PluginInstrument`, marshals to the message thread, opens or reuses one window per slot, and closes it before the source is unloaded or evicted. Add `POST /patches/{slot}/editor` and `DELETE /patches/{slot}/editor`; the Instrument page's "Open editor" calls the API instead of rendering the descriptor-only details. The editor is never opened automatically when a patch is selected or armed; only the explicit control opens it. This must work for in-process VST3 and for yabridge-wrapped Arturia plugins, and report a clear error when an editor cannot be created.
- files: `engine/src/Session.{h,cpp}`, `engine/src/plugins/PluginWindow.{h,cpp}`, `engine/src/api/`, `ui/src/features/instrument/`, `shared/api/openapi.yaml`, `tests/`
- accept: `POST /patches/{slot}/editor` opens a native window for `TestPlugin` and for Analog Lab V when installed; the window can be closed and reopened idempotently; switching/unloading the patch closes it before the processor is destroyed; `ctest` and the UI snapshots stay green.

## T130 · Per-patch plugin state and editor follow
- depends: T119, T128, T129
- size: M
- goal: Give every plugin patch its own persistent state. Capture `getStateInformation()` when the patch is switched away from, when its editor closes and before warm-pool eviction; restore it when the instance is created and whenever the patch is armed again. Persist the blobs in a sidecar next to `set.yaml` and load them at startup. When the armed patch changes, close editor windows belonging to non-armed slots so an open editor never keeps showing the old patch; opening the editor for the new patch shows its own restored state.
- files: `engine/src/Session.{h,cpp}`, `engine/src/api/SessionBridge.{h,cpp}`, `engine/src/plugins/PluginWindow.{h,cpp}`, `tests/`
- accept: assign Analog Lab V to two patches, give them different presets, switch back and forth and restart the app; each patch restores its own sound, the old editor does not stay bound to the previous patch, and `ctest` stays green.

## T131 · 64-patch state and Analog Lab acceptance
- depends: T130
- size: M
- goal: Two headless layers. Automated: 8 families x 8 patches using TestPlugin, each patch with its own state; arm all 64, switch across them so the warm pool evicts and reloads, restart and verify every state hash. Editor-follow is already covered by T130's `ihub-session-editor-follow`; do not open editors or wait on the message loop in this test. Real plugin: a smaller, stable Analog Lab pass - at least 4 slots with 2 distinct captured Analog Lab states, switch A -> B -> A, restart, verify the hashes and that both patches render non-silent, different audio, again without editor windows. Do not load Analog Lab 64 times in one process: the Wine bridge stack-overflows under rapid repeated load. The full 64-patch Analog Lab pass stays a documented manual check in `docs/`.
- files: `tests/e2e/`, `tests/CMakeLists.txt`, `docs/`
- accept: the automated 64-slot TestPlugin test and the 4-slot Analog Lab test both pass through `scripts/test.sh` (120 s per-test timeout), with state-hash checks before/after eviction and after restart, and a recorded non-silent/different-audio result for the Analog Lab pair; full `ctest` green; the manual 64-patch Analog Lab procedure is documented and does not hang.

## T132 · Explicit save-sound control
- depends: T130
- size: S
- goal: Add a visible "Save sound to patch" action on the Instrument page and `POST /patches/{slot}/state/capture`, which immediately captures the running plugin's state into that patch and confirms it. Auto-capture on switch/editor close/eviction stays; this is for tweaking a sound and saving it without switching away.
- files: `engine/src/api/`, `ui/src/features/instrument/`, `shared/api/openapi.yaml`, `tests/`
- accept: change a preset, press Save, switch away and back, restart and verify the patch restores; `ctest`, typecheck and snapshots stay green.

## T133 · Architecture map and call paths
- depends: T132
- size: S
- goal: Write `docs/architecture.md`: module responsibilities, dependency direction, and the exact call paths for (a) Launchpad pad -> arm -> audio, (b) UI click -> `/arm` -> session, (c) patch edit -> plugin state capture, (d) the audio callback and warm pool. List the oversized files and their target decomposition, and a "start reading here" path for a new contributor. Link it from `HANDOVER.md`.
- files: `docs/architecture.md`, `HANDOVER.md`
- accept: the document covers every directory under `engine/src`, all four call paths, and names the files to split; Markdown links resolve; no code changes.

## T134 · Split the mapping layer
- depends: T133
- size: M
- goal: Preserve behavior with characterization tests, then split `engine/src/controller/Mapping.cpp` (~2000 lines) into a YAML parser, a validator, the `SetMapping` model and a watcher/facade. No logic rewrites; move code.
- files: `engine/src/controller/Mapping.{h,cpp}`, new controller files, `tests/controller/MappingTest.cpp`
- accept: no file above ~800 lines; every mapping test passes unchanged; hot reload, validation errors and bounds behave exactly as before; `ctest` green.

## T135 · Split the API routes by resource
- depends: T134
- size: L
- goal: Split `engine/src/api/Routes.cpp` (~2100 lines) into small route modules (state, patches, audio, MIDI, plugins, transport, pairing) registered through one helper. `Server` keeps transport, auth and the WebSocket loop. No endpoint, JSON or status-code changes.
- files: `engine/src/api/Routes.{h,cpp}`, new `api/routes/*`, `tests/api/`
- accept: `shared/api/openapi.yaml` unchanged; all API tests pass; each route module under ~600 lines; `ctest` green.

## T136 · Decompose Session
- depends: T135
- size: L
- goal: Keep `Session` as a thin facade and extract collaborators: warm-pool management, per-patch plugin state, editor windows, meter bridge and transport control. Preserve the audio-thread contract and all public behavior.
- files: `engine/src/Session.{h,cpp}`, new `engine/src/session/*`, `tests/`
- accept: `Session.cpp` under ~800 lines; the 64-slot, warm-pool, editor, state-persistence and e2e tests pass; `--rt-strict` reports no allocations/locks in the callback; `ctest` green.

## T137 · Split Main into app modes and shell
- depends: T136
- size: M
- goal: Extract diagnostics, selftest, snapshot and serve modes out of `engine/src/Main.cpp` (~2000 lines), leaving an entry point and UI shell. Every command-line mode keeps its exact behavior and output.
- files: `engine/src/Main.cpp`, new `engine/src/app/*`, tests
- accept: `--diagnostics`, `--selftest`, `--snapshot`, `--serve`, `--list-devices` and `--session-smoke` behave as before; `Main.cpp` under ~600 lines; `ctest` green.

## T138 · Remove superseded code
- depends: T137
- size: S
- goal: Remove or clearly quarantine the superseded Launchkey controller layer and its docs/tests, remove duplicate controller docs, and either wire `project/PluginStateStore` into the live session or delete it. Update AGENTS/DECISIONS/HANDOVER references so only the Launchpad path remains.
- files: `engine/src/controller/`, `engine/src/project/PluginStateStore.*`, `tests/`, docs
- accept: the build has no unused controller sources; `ctest` green; no document or code path still presents Launchkey as the target device.
