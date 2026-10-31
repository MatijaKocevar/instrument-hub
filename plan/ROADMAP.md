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
