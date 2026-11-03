# Roadmap — task queue

One task = one session. Format is parsed by `autopilot.py`; keep the
`## T0xx ·` headers and the `- key:` lines intact.

Rules for every task: implement, run the acceptance check, write the result
file. If the check fails you are not done.

Bugs are reproduced before they are fixed. Every bug task follows
`plan/REPRO.md`: a Playwright run (UI) or a `curl` pair (engine/API) that
captures the verbatim error, recorded in `plan/BUGS.md`, before any source
edit.

> **Phase (2026-11-01):** the feature roadmap is complete and the app is
> playable on this machine. New work is **bug fixes only**, queued from
> `plan/BUGS.md` after reproduction. Do not add speculative features or
> refactors to this file without an explicit human request.
>
> The single-responsibility refactor queue **T140–T174** was added by an
> explicit human request; see `plan/REFACTOR.md` for the rationale and
> target structure. **T182** (Prettier for the UI), **T187** (native JACK
> device type), **T188–T192** (settings persistence, design in
> `plan/SETTINGS.md`), **T193–T195** (device list, design in
> `plan/DEVICES.md`) and **T196–T205** (the human-reported bug list of
> 2026-11-03) are likewise explicit human requests. Machine settings and
> projects are separate stores; see `plan/PROJECTS.md`.

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

## T131 · Per-patch state acceptance
- depends: T130
- size: M
- goal: Prove per-patch state without a plugin stress test. Use the normal asynchronous warm pool; never add a synchronous plugin-loading mode, because JUCE plugin instantiation posts to the message thread and blocks. In each test, call `armSlot(slot)`, then pump `juce::MessageManager::getInstance()->runDispatchLoopUntil(10)` in a bounded loop until `getWarmState(slot) == active` or a 5 s deadline; fail with the state name on timeout. Automated: one patch per family (8 slots) with TestPlugin, two distinct states; arm and switch so the warm pool evicts and reloads, restart, and verify every state hash. No editors and no `PluginWindow`. Real plugin: 2 Analog Lab patches with 2 captured states; switch A -> B -> A and restart, verify state hashes and non-silent/different audio, headless. Document the full 64-pad Analog Lab pass as a manual checklist in `docs/manual-analog-lab-64.md`; do not automate 64 Wine-hosted instances.
- files: `tests/e2e/`, `tests/CMakeLists.txt`, `docs/`
- accept: both tests pass through `scripts/test.sh` inside the 120 s per-test timeout; every arm is bounded by the message-pump deadline; no synchronous mode, no blocking plugin creation on the message thread and no editors; no process loads more than two Analog Lab instances; full `ctest` green; the manual 64-pad checklist exists.

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

## T139 · App sidebar and settings/account navigation
- depends: T138
- size: M
- goal: Replace the in-page route buttons with a persistent left sidebar in the desktop shell. Workspace items: Grid, Instrument, Plugins, Audio & MIDI. A bottom group holds Settings & Account: the existing pairing code, LAN mode enable/disable, token revoke, and an About/version section (no login system is invented). Active route is highlighted; `/remote` keeps its own full-screen layout without a sidebar. On narrow/tablet widths collapse to an icon rail or bottom tabs. Page-specific actions stay in the page; route navigation moves out of page bodies. Add the `/settings` route to the router and move the pairing panel there.
- files: `ui/src/App.tsx`, `ui/src/state/router.ts`, `ui/src/features/pairing/`, `ui/src/styles.css`, `ui/src/testing/snapshot.tsx`
- accept: a 1200x800 snapshot shows the sidebar with the four workspace items and a Settings & Account group, with the active route highlighted; clicking each renders the page; `/settings` exposes pairing/LAN/account/about; no duplicate route buttons remain in page headers; a narrow snapshot shows the collapsed rail/bottom tabs; snapshot and typecheck stay green.

---

## Refactor phase (human-requested, 2026-11-01)

These tasks apply the single-responsibility rule in `AGENTS.md` to the
existing code. Every task is a pure move: no behaviour, API, JSON or test-id
changes. See `plan/REFACTOR.md` for the rationale and target structure.

## T140 · Remove the global header and connection bar
- depends: T139
- size: S
- goal: Fix `plan/BUGS.md` BUG-003. Delete the repeated "Audio workspace / Instrument Hub / One place…" hero header and the Connected / URL / Retry now bar from the desktop shell. Keep connection state in the sidebar and footer. Update the native `--snapshot` / `--ui-check` probe so it no longer waits for the removed `app-title` marker.
- files: `ui/src/App.tsx`, `ui/src/styles.css`, `ui/src/testing/snapshot.tsx`, `engine/src/app/UiModes.{h,cpp}`
- accept: no desktop route renders `app__header` or `data-testid="connection-status"`; the page harness asserts their absence; `pnpm typecheck` and `pnpm test:snapshot` pass; `ihub-ui-snapshot` still captures the Grid.

## T141 · Split the desktop shell
- depends: T140
- size: M
- goal: Reduce `ui/src/App.tsx` to a thin router entry point. Move navigation, shell layout, shell state/effects, route selection and the Grid summary panels into `ui/src/features/shell/`, one responsibility per file.
- files: `ui/src/App.tsx`, new `ui/src/features/shell/`
- accept: `App.tsx` only chooses between `/remote` and the desktop shell; every shell file passes the one-sentence test; no behaviour or test-id changes; `pnpm typecheck` and `pnpm test:snapshot` pass.

## T142 · Split the snapshot harness: fixtures and server
- depends: T140
- size: L
- goal: Move the fixture data and the fixture HTTP server out of `ui/src/testing/snapshot.tsx` into focused modules under `ui/src/testing/`. The harness should import them; no scenario assertions move in this task.
- files: `ui/src/testing/snapshot.tsx`, new `ui/src/testing/fixtures/`, new `ui/src/testing/fixtureServer.ts`
- accept: the fixture server behaves exactly as before; `pnpm typecheck` and `pnpm test:snapshot` pass with the same checks.

## T143 · Split the snapshot harness: scenarios
- depends: T142
- size: L
- goal: Move each area's scenarios and assertions out of `snapshot.tsx` into per-feature scenario modules (shell, grid, instrument, plugins, audio, MIDI, pairing, remote, store). Keep one small runner that calls them in the existing order.
- files: `ui/src/testing/`
- accept: the harness prints the same PASS lines; no fixture or assertion lives in another area's file; `pnpm test:snapshot` passes.

## T144 · Split the UI store: lifecycle and polling
- depends: T141
- size: M
- goal: Separate store subscription, snapshot patching, refresh/poll scheduling and retry/backoff from the domain actions in `ui/src/state/store.ts`.
- files: `ui/src/state/store.ts`, new `ui/src/state/`
- accept: the public store API is unchanged; `pnpm typecheck` and `pnpm test:snapshot` pass.

## T145 · Split the UI store: patches and FX
- depends: T144
- size: L
- goal: Move patch arm/update/colour/source/editor/capture actions and the FX-chain actions into their own domain modules with the same optimistic-update behaviour.
- files: `ui/src/state/`
- accept: optimistic arm, patch and FX behaviour is unchanged; the store tests and snapshots pass.

## T146 · Split the UI store: audio, MIDI, plugins, pairing, transport
- depends: T145
- size: M
- goal: Move the remaining domain actions out of the core store so each file owns one API area.
- files: `ui/src/state/`
- accept: the public store API is unchanged; `pnpm test:snapshot` passes.

## T147 · Split the UI API client: transport and state
- depends: T146
- size: M
- goal: Keep one transport/error core in `ui/src/api/client.ts` and move the state, arm, transport and patch endpoints into a domain module with the same exported types.
- files: `ui/src/api/client.ts`, new `ui/src/api/`
- accept: all existing named exports still resolve from `ui/src/api/client`; `pnpm typecheck` and `pnpm test:snapshot` pass.

## T148 · Split the UI API client: FX, plugins, MIDI, audio, pairing
- depends: T147
- size: M
- goal: Move the remaining endpoint groups out of the client core, one file per resource.
- files: `ui/src/api/`
- accept: the public client surface is unchanged; `pnpm typecheck` and `pnpm test:snapshot` pass.

## T149 · Split the Instrument page: frame and mixer
- depends: T141
- size: M
- goal: Move the page frame and the level/pan/mute/solo mixer out of `ui/src/features/instrument/InstrumentPage.tsx`.
- files: `ui/src/features/instrument/`
- accept: the instrument snapshot checks and test ids are unchanged; `pnpm test:snapshot` passes.

## T150 · Split the Instrument page: source and inserts
- depends: T149
- size: L
- goal: Move the source card, source picker, editor controls and insert chain out of `InstrumentPage.tsx` into focused components.
- files: `ui/src/features/instrument/`
- accept: source/insert behaviour and latency readouts are unchanged; the instrument snapshot checks pass.

## T151 · Split the Instrument page: save and capture
- depends: T150
- size: M
- goal: Move the explicit save-sound action and its confirmation/error states out of the page, leaving the page as a composition of cards.
- files: `ui/src/features/instrument/`
- accept: the save-sound snapshot checks pass; `pnpm test:snapshot` is green.

## T152 · Split the grid view and meters
- depends: T141
- size: M
- goal: Separate grid layout, cell rendering, colour swatch handling, meter transport and pure model logic where they are entangled in `features/grid/`.
- files: `ui/src/features/grid/`
- accept: the grid, meter and loading snapshot checks pass unchanged.

## T153 · Split the remote page: layout and set list
- depends: T141
- size: M
- goal: Move the stage layout, set list, now-playing card and patch stepping out of `ui/src/features/remote/RemoteView.tsx`.
- files: `ui/src/features/remote/`
- accept: the remote snapshot checks and test ids are unchanged.

## T154 · Split the remote page: controls and link
- depends: T153
- size: M
- goal: Move transport/panic controls, the reconnect banner, wake-lock handling and the live meter link into focused modules.
- files: `ui/src/features/remote/`
- accept: the remote transport, banner and wake-lock checks pass unchanged.

## T155 · Split the audio settings panel
- depends: T141
- size: M
- goal: Separate device selection, channel maps, latency readout, xruns and measurement from `ui/src/features/audio-settings/AudioSettingsPanel.tsx`.
- files: `ui/src/features/audio-settings/`
- accept: the audio settings snapshot checks are unchanged.

## T156 · Split the MIDI panel
- depends: T141
- size: M
- goal: Separate the input list, port status, and pad test from `ui/src/features/midi/MidiPanel.tsx`.
- files: `ui/src/features/midi/`
- accept: the MIDI snapshot checks are unchanged.

## T157 · Split the plugins page
- depends: T141
- size: M
- goal: Separate the toolbar/search, inventory list, plug-in row/card and used-by hints from `ui/src/features/plugins/PluginsPage.tsx`.
- files: `ui/src/features/plugins/`
- accept: the plugins snapshot checks are unchanged.

## T158 · Split the settings and pairing panels
- depends: T141
- size: M
- goal: Separate the account card, about card and the pairing panel's QR/code/token sections into focused components.
- files: `ui/src/features/settings/`, `ui/src/features/pairing/`
- accept: the settings and pairing snapshot checks are unchanged.

## T159 · Remaining UI module sweep
- depends: T151, T152, T154, T155, T156, T157, T158
- size: M
- goal: Apply the one-sentence test to the remaining UI modules and split any file that still mixes responsibilities. Do not rewrite working code for style alone.
- files: `ui/src/`
- accept: every touched file has one reason to change; `pnpm typecheck` and `pnpm test:snapshot` pass.

## T160 · Split the API server: route families
- depends: T139
- size: L
- goal: Separate the REST route families from `engine/src/api/Server.cpp` into focused route modules, keeping `Server` as transport/auth/facade.
- files: `engine/src/api/`
- accept: `shared/api/openapi.yaml` is unchanged; the API tests pass unchanged.

## T161 · Split the API server: auth, state, meters and WebSocket
- depends: T160
- size: L
- goal: Separate pairing/auth checks, state serialization, meter sampling and WebSocket broadcast from the server core.
- files: `engine/src/api/`
- accept: the pairing, state-broadcast, meter and session API tests pass unchanged.

## T162 · Split the FX rack
- depends: T139
- size: M
- goal: Separate insert lifecycle, parameter reads, latency budgeting and reordering in `engine/src/audio/FxRack.cpp`.
- files: `engine/src/audio/FxRack.{h,cpp}`
- accept: `ihub-fx-chain` and `ihub-plugin-fx` pass unchanged.

## T163 · Split the warm pool and controller
- depends: T139
- size: M
- goal: Separate slot reservation, warm-up state, eviction and session orchestration in `audio/WarmPool.cpp` and `session/WarmPoolController.cpp`.
- files: `engine/src/audio/WarmPool.{h,cpp}`, `engine/src/session/WarmPoolController.{h,cpp}`
- accept: the warm-pool, session-warm and switch-continuity tests pass unchanged.

## T164 · Split project serialization
- depends: T139
- size: M
- goal: Separate set metadata, slot/patch data, plugin-state blobs and version migration in `engine/src/project/Serializer.cpp`.
- files: `engine/src/project/Serializer.{h,cpp}`
- accept: the set-file, migrations and patch-persistence tests pass unchanged.

## T165 · Split the plugin host and sandbox
- depends: T139
- size: L
- goal: Separate scanning/hosting, sandbox process transport, parameter bridging and editor-window lifetime across `engine/src/plugins/`.
- files: `engine/src/plugins/`
- accept: the plugin instance, window, scanner, cache and TestPlugin scan tests pass unchanged.

## T166 · Split the sampler
- depends: T139
- size: M
- goal: Separate kit parsing, sample loading/decoding and voice rendering across `engine/src/sampler/`.
- files: `engine/src/sampler/`
- accept: the kit-loader, sample-loader and sampler-instrument tests pass unchanged.

## T167 · Split the controller and mapping code
- depends: T139
- size: L
- goal: Separate protocol encoding, pad mapping, validator diagnostics and YAML parsing in `engine/src/controller/`.
- files: `engine/src/controller/`
- accept: the Launchpad and mapping tests pass unchanged.

## T168 · Split diagnostics, audio settings and device management
- depends: T139
- size: M
- goal: Separate each diagnostic mode and split device enumeration, channel maps and latency calculation in `engine/src/app/` and `engine/src/audio/`.
- files: `engine/src/app/`, `engine/src/audio/`
- accept: the app-mode, diagnostics, audio-settings and device-manager tests pass unchanged.

## T169 · Remaining engine module sweep
- depends: T161, T162, T163, T164, T165, T166, T167, T168
- size: M
- goal: Apply the one-sentence test to the remaining engine files and split any that still mix responsibilities. Do not rewrite working code for style alone.
- files: `engine/src/`
- accept: every touched file has one reason to change; `scripts/test.sh` is green.

## T170 · Split the end-to-end tests
- depends: T139
- size: M
- goal: Separate the click/pad/LED/audio scenarios and shared rig setup from `tests/e2e/PadToAudio.cpp` and `tests/e2e/AnalogLabStateTest.cpp`.
- files: `tests/e2e/`
- accept: the same scenarios run and pass.

## T171 · Split the controller and sampler tests
- depends: T139
- size: M
- goal: Group cases by contract (protocol, mapping, validation, kit, sample, voices) instead of one large test file per area.
- files: `tests/controller/`, `tests/sampler/`
- accept: the same checks run and pass.

## T172 · Split the DSP and audio tests
- depends: T139
- size: M
- goal: One test file per effect or behaviour concern across `tests/dsp/` and `tests/audio/`.
- files: `tests/dsp/`, `tests/audio/`
- accept: the same checks run and pass.

## T173 · Split the API and session tests
- depends: T139
- size: M
- goal: Separate route-group cases and shared session fixtures from `tests/api/`.
- files: `tests/api/`
- accept: the same checks run and pass.

## T174 · Remaining test sweep
- depends: T170, T171, T172, T173
- size: M
- goal: Apply the one-sentence test to the remaining test files, splitting by scenario and moving fixtures into their own files.
- files: `tests/`
- accept: every touched file has one reason to change; `scripts/test.sh` is green.

---

## Comment cleanup phase (human-requested, 2026-11-01)

First-party code must explain itself. Remove every comment that is not a
directive required by a tool, language, framework or platform. Rationale
belongs in `docs/` or `plan/DECISIONS.md`, not in source. Exempt:
`third_party/`, generated code, `docs/` and `plan/`.

## T175 · Inventory and classify comments
- depends: T174
- size: S
- goal: Research the whole first-party tree (`engine/`, `ui/src/`, `tests/`, `scripts/`, build files). Count comments per area, list every directive pattern that must survive (NOLINT, clang-format, eslint, ts-expect-error, prettier-ignore, noqa, generated markers, shebangs), and flag risky cases such as `//` inside strings, URLs and regexes. Write the inventory and removal rules to `plan/COMMENTS.md`. Make no source edits.
- files: `plan/COMMENTS.md`
- accept: the report covers every first-party source directory, lists the surviving directive patterns with examples, and gives a per-area comment count; no source file is modified.

## T176 · Remove comments from the UI
- depends: T175
- size: M
- goal: Remove all non-directive comments from `ui/src/`. Keep only the directives listed in `plan/COMMENTS.md`; do not change code, strings, URLs or regexes.
- files: `ui/src/`
- accept: `pnpm typecheck` and `pnpm test:snapshot` pass; only directive comments remain in `ui/src/`; no behaviour or markup change.

## T177 · Remove comments from the engine API and session
- depends: T175
- size: M
- goal: Remove all non-directive comments from `engine/src/api/`, `engine/src/session/` and `engine/src/project/`. Keep only tool/framework directives.
- files: `engine/src/api/`, `engine/src/session/`, `engine/src/project/`
- accept: `scripts/test.sh` passes; only directive comments remain in those directories; no behaviour or API change.

## T178 · Remove comments from engine audio, DSP and instruments
- depends: T175
- size: M
- goal: Remove all non-directive comments from `engine/src/audio/`, `engine/src/dsp/`, `engine/src/instruments/` and `engine/src/sampler/`. Keep only directives.
- files: `engine/src/audio/`, `engine/src/dsp/`, `engine/src/instruments/`, `engine/src/sampler/`
- accept: `scripts/test.sh` passes; only directive comments remain in those directories; the audio-thread contract is unchanged.

## T179 · Remove comments from the rest of the engine
- depends: T175
- size: M
- goal: Remove all non-directive comments from `engine/src/controller/`, `engine/src/plugins/`, `engine/src/app/`, `engine/src/Bridge/` and the remaining engine sources and headers. Keep only directives.
- files: `engine/src/`
- accept: `scripts/test.sh` passes; only directive comments remain under `engine/src/`; no behaviour change.

## T180 · Remove comments from tests, scripts and build files
- depends: T175
- size: M
- goal: Remove all non-directive comments from `tests/`, `scripts/` and the first-party CMake files. Keep only tool directives and shebangs.
- files: `tests/`, `scripts/`, `CMakeLists.txt`, `CMakePresets.json`, `engine/CMakeLists.txt`, `tests/CMakeLists.txt`
- accept: `scripts/test.sh` passes; only directive comments and shebangs remain; no build or test behaviour change.

## T181 · Final comment audit
- depends: T176, T177, T178, T179, T180
- size: S
- goal: Walk the whole first-party tree and remove any non-directive comment that survived. Verify that strings, URLs, regexes and generated markers were not damaged. Record the final counts and every exemption in `plan/COMMENTS.md`.
- files: `engine/`, `ui/src/`, `tests/`, `scripts/`, build files, `plan/COMMENTS.md`
- accept: a search of the first-party tree finds only the directives listed in `plan/COMMENTS.md`; `pnpm typecheck`, `pnpm test:snapshot` and `scripts/test.sh` pass; no source behaviour changed.

## T182 · Adopt Prettier for the UI
- depends: T181
- size: M
- goal: Make Prettier the single formatting authority for `ui/`: add `prettier` to `devDependencies`, add `ui/.prettierrc` and `ui/.prettierignore` (skip `dist/`, `dist-test/`, `node_modules/` and generated `src/api/schema.d.ts`), add `format` and `format:check` scripts, format the UI tree, and run `format:check` in CI. Use Prettier defaults (2-space indent, K&R braces); the current hand-written Allman style is intentionally replaced, and no brace-style plugin is added.
- files: `ui/package.json`, `ui/pnpm-lock.yaml`, `ui/.prettierrc`, `ui/.prettierignore`, `ui/src/`, `ui/vite.config.ts`, `ui/vite.snapshot.config.ts`, `.github/workflows/`
- accept: `pnpm format:check`, `pnpm typecheck` and `pnpm test:snapshot` pass; the diff is formatting only; no behaviour, markup or generated-file change.

## T183 · BUG-001 `/audio` reports `open=false` while the session is streaming
- depends: T182
- size: S
- goal: Make `/audio` report the session's live device. Root cause: `ControlApi` creates a second `AudioSettingsBridge` with its own `AudioEngine::DeviceManager`, while `Session` opens and streams through a different manager, so `snapshot().open` is false and the name comes from the unused bridge. Inject `session->getDevices()` into the bridge, keep the owned manager as the fallback for `--serve`/`--audio`, and keep the latency probe on its own manager so measuring does not close the live session device. Add a regression in `tests/api/audio_settings/DeviceBridge.cpp` that opens the Loopback on an external manager, injects it and asserts `open`, device name, rate and buffer follow that manager.
- files: `engine/src/audio/AudioSettings.h`, `engine/src/audio/AudioSettings.cpp`, `engine/src/audio/settings/`, `engine/src/app/ControlApi.cpp`, `tests/api/audio_settings/`
- accept: `scripts/test.sh` passes; the new test fails before the wiring and passes after; `plan/BUGS.md` BUG-001 gains the fix commit and evidence.

## T184 · BUG-005 engine aborts when the Wine plugin bridge dies
- depends: T182
- size: L
- goal: Make a dead `vstbridge-host` a failed plugin slot instead of an engine `SIGABRT`. `SessionBuild.cpp` still loads every plugin slot as an in-process `plugins::PluginInstrument`; `ihub-sandbox`/`SandboxHost` implement crash isolation but are used only by their own test. Route plugin slots through that isolation (or an equivalent boundary) while keeping editor, per-patch state, latency reporting and the audio-thread contract intact. Reproduce first with the BUG-005 steps: arm an Analog Lab patch, `kill -9` its host, and confirm the current abort; then add a regression that kills the host mid-render and asserts the host process survives and the slot reports an error.
- files: `engine/src/session/SessionBuild.cpp`, `engine/src/plugins/`, `engine/src/session/EditorHost.cpp`, `tests/plugins/`, `tests/`
- accept: `scripts/test.sh` passes; the new host-death regression proves the engine process does not abort; plugin state and editor tests stay green; no allocation, locks or logging on the audio thread.
- note: if full sandbox routing is too large for one session, land the smallest isolation that removes the in-process abort and record the remaining gap in `plan/BUGS.md`; do not mark done on a smoke test.

## T185 · BUG-004 app hangs after the window is closed
- depends: T182
- size: L
- goal: Make shutdown complete within a second after the last window closes. BUG-004 evidence shows the main thread in `futex_wait`, one thread at 100% CPU, the control server already stopped and `WM_DELETE_WINDOW` ignored. Audit the shutdown order in `engine/src/Main.cpp`, `engine/src/api/Server.cpp`, `engine/src/app/ControllerSurface.cpp`, `Session::closeDevice`, `WarmPoolController::shutdown`, `MeterSampler::stop` and the plugin/editor threads; remove the join cycle or unbounded wait. Add a regression that constructs the session and control API, starts and stops them, and fails if shutdown takes longer than 2 s; use a hard test timeout so a regression cannot hang the suite.
- files: `engine/src/app/`, `engine/src/api/`, `engine/src/session/`, `tests/`
- accept: `scripts/test.sh` passes; the new shutdown regression completes in under 2 s; `ctest --timeout 120` stays green; no audio-thread changes.
- note: the original hang involved Analog Lab; if it cannot be reproduced without the real plugin, prove the deadlock with the closest fake or TestPlugin harness and record what remains in `plan/BUGS.md`.

## T186 · BUG-002 full 64-patch Analog Lab workflow is only partly automated
- depends: T184, T182
- size: L
- goal: Make the 64-patch workflow survive the Wine bridge by isolating plugin state batches so 64 Analog Lab instances never accumulate in one bridge process. Use the child-process-per-batch candidate from BUG-002, reuse the T184 isolation boundary if it landed, and keep the existing 2-patch and 8-patch tests green. Add a 64-patch TestPlugin batch regression that proves every patch restores its own preset; real Analog Lab validation remains the human task B003.
- files: `tests/e2e/analog_lab/`, `tests/`, `scripts/`
- accept: `scripts/test.sh` passes; the 64-patch TestPlugin batch test passes; `plan/BUGS.md` BUG-002 records what is automated and what still needs Analog Lab; the engine changes stay within the T184 isolation.
- note: if deterministic batches need the real plugin, write `STATUS: blocked` and point at B003 with the exact remaining step.

## T187 · Native JACK audio device type
- depends: T183
- size: M
- goal: Build JUCE with `JUCE_JACK=1` so a native JACK `AudioIODeviceType` is offered next to ALSA, and keep ALSA the default. JUCE needs the JACK headers at compile time but `dlopen`s `libjack.so.0` at runtime (`third_party/JUCE/modules/juce_audio_devices/native/juce_JackAudio.cpp`), so the build needs only the `jack` dev headers already listed in `docs/new-machine.md`, no linker flag. Set the define on `instrument-hub` and on the device-manager test target; console targets that never open an audio device do not need it. Do not change `AudioSettingsBridge::firstDeviceType()`: ALSA stays the default so the Loopback rig and every existing test are unaffected, while JACK appears in `deviceTypes`, in `GET /audio/devices` and on the Audio page and can be selected explicitly. A missing `jackd` must degrade to "JACK type present, no devices", not to a failure. Add a device-manager regression in `tests/device_manager/` that asserts `getDeviceTypeNames()` contains both `ALSA` and `JACK` and that JACK reports no output devices when no server is reachable; the existing audio-settings test keeps proving ALSA is still the default.
- files: `engine/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/device_manager/`, `docs/architecture.md`
- accept: `scripts/test.sh` passes; `ihub-device-manager-test` prints both device types; `--list-devices` still lists the Loopback; the app still opens ALSA at 48 kHz/128 by default; no audio-thread changes.
- note: no `jackd` runs on this machine, so the live JACK open path cannot be exercised. Record the untested path (start `jackd`, select JACK under `/audio`) in the result and in `docs/architecture.md`; do not block on it. The ALSA `jack` PCM that ALSA enumeration already probes is not the native JACK type, so both may appear in the ALSA device list.

## Bug list 2026-11-03 (human-reported, UI and controller)

Each task reproduces its bug first per `plan/REPRO.md` (Playwright for UI,
LED capture for the controller) and records the verbatim evidence in
`plan/BUGS.md` before touching source.

## T196 · BUG-008 align grid rows with the Launchpad
- depends: none
- size: M
- goal: Make the screen grid match the hardware: slot index 0 (pad note 11) is the bottom-left cell and the first family row is the bottom row. Reverse the render order in the grid model/view rather than remapping slots, keep column order and all slot-keyed callbacks unchanged, and update the snapshot fixtures.
- files: `ui/src/features/grid/model.ts`, `ui/src/features/grid/GridView.tsx`, `ui/src/features/grid/GridTable.tsx`, `ui/src/testing/`
- accept: with `/grid` open at 1200x800, the first rendered `grid-cell` row holds slots 57–64 and the last holds 0–7; clicking any cell still arms its own slot; the virtual-Launchpad capture for note 11 lights slot 0; `pnpm test:snapshot` passes.


## T197 · BUG-009 reserve red for the armed patch
- depends: none
- size: M
- goal: Remove red from the patch palette on both sides and keep the two palettes aligned, so only the armed patch is red. Replace the rose entries in `PATCH_COLOURS`, replace the red Launchpad velocities (including index 0's velocity 5) with non-red ones in the same order, and give empty cells a neutral gradient instead of the index-0 colour.
- files: `ui/src/features/grid/colours.ts`, `ui/src/features/grid/GridCell.tsx`, `engine/src/controller/LaunchpadProtocol.h`, `engine/src/controller/LaunchpadLed.cpp`, `tests/controller/`, `tests/e2e/`
- accept: a 16-index sweep produces no red velocity from `velocityForPatchColour`; after arming one pad the virtual-Launchpad capture contains exactly one red LED; the UI palette has no red hue and empty cells are neutral; `scripts/test.sh` and `pnpm test:snapshot` pass.


## T198 · BUG-010 push armed-state changes to the UI
- depends: none
- size: L
- goal: Stop making the UI wait for the 2 s poll. Broadcast the control-state revision over the existing WebSocket whenever the snapshot changes (arm, patch edit, warm state, device), apply it in the store, and keep polling as the fallback with the current backoff. Do not add a second socket.
- files: `engine/src/api/WebSocketRoutes.cpp`, `engine/src/api/StateBroadcast.cpp`, `engine/src/api/state/`, `ui/src/state/store.ts`, `ui/src/state/pollLoop.ts`, `ui/src/features/shell/useShellData.ts`, `tests/api/`
- accept: with `/grid` open, a `POST /arm` issued outside the browser moves `grid-cell[data-armed="true"]` within 250 ms; with the socket aborted the poll still applies it within 3 s; `scripts/test.sh` and `pnpm test:snapshot` pass.


## T199 · BUG-011 make the `.ihub` project the live set
- depends: none
- size: L
- goal: Make the project container the runtime format, per `plan/PROJECTS.md`. Resolve the project path (`--project` > `~/Music/Instrument Hub/Untitled.ihub`); if it does not exist, import the bundled `shared/set.yaml` plus `shared/set.plugin-state/` through `Serializer::save` once. Load the project with `Serializer::load`, parse `SetFile::mappingYaml` with the existing mapping parser (add a string entry point; do not write a temp file), and build the session from it. Watch the project file's mtime instead of `set.yaml` for hot reload. No saving yet; the last-opened path is T205.
- files: `engine/src/app/SessionSetup.cpp`, `engine/src/project/Serializer.cpp`, `engine/src/project/serializer/`, `engine/src/controller/mapping/MappingFile.cpp`, `engine/src/controller/mapping/MappingYaml.cpp`, `tests/project/`, `tests/e2e/`
- accept: launching with `--project` on a temp `.ihub` produces the same 64 pads, families, colours and plug-in state as the imported `set.yaml`; deleting the `.ihub` and relaunching re-imports once; replacing the `.ihub` externally re-routes pads within the watcher interval; `scripts/test.sh` passes.


## T204 · BUG-011 autosave the project
- depends: T199
- size: L
- goal: Save the project after accepted edits: patch and family mixer, colour, source, FX chain, transport and a captured plug-in sound. Capture the running plug-in state for touched slots immediately before each write, debounce 500 ms, write `<name>.ihub.tmp` and rename, keep the previous file as `<name>.ihub.bak`, and expose a `dirty` flag plus the last save error so the footer can show it. A read-only project keeps running with `saved: false`. Restoring app settings never writes the project.
- files: `engine/src/api/state/StateStorePatches.cpp`, `engine/src/api/StateStore.*`, `engine/src/project/Serializer.cpp`, `engine/src/session/PluginStateFile.cpp`, `engine/src/Session.cpp`, `tests/api/patch/`, `tests/project/`
- accept: an edit followed by a 1 s wait and a restart loads the edited value; a burst of ten fader moves produces one file write; a simulated write failure leaves the previous `.ihub` byte-identical and returns the error; a crash mid-write never leaves a partial `.ihub`; `scripts/test.sh` passes.


## T200 · BUG-012 keep audio alive while the plug-in editor changes sound
- depends: none
- size: L
- goal: Reproduce the silence first and capture the log, then make editor-side sound changes reach the sandboxed audio instance without closing the editor. The editor mirror (T184) is the only path into the sandbox today, so either push parameter/state changes to it on edit or re-arm the slot in place; audio must never stop when a preset changes.
- files: `engine/src/session/EditorHost.cpp`, `engine/src/plugins/`, `engine/src/session/SessionBuild.cpp`, `tests/plugins/`, `tests/e2e/`
- accept: with a TestPlugin mirror, changing a parameter while the editor is open keeps the slot rendering above -40 dBFS and the new value is audible in the next render; the engine log shows no device close/reopen; `scripts/test.sh` passes. The real Analog Lab check is recorded for the human.
- note: if the fix needs the real plug-in, prove the mechanism with TestPlugin and write `STATUS: partial` with the remaining human step rather than `done`.


## T201 · BUG-013 stop arming from recolouring the other pads
- depends: none
- size: M
- goal: Every pad keeps its patch colour; arming changes only the armed pad (static red). Show warming with a pulsing mode and error with a flashing red instead of replacing the colour of every pad that changes warm state.
- files: `engine/src/app/ControllerSurface.cpp`, `engine/src/controller/LaunchpadLed.cpp`, `engine/src/controller/launchpad/LaunchpadDriverLeds.cpp`, `tests/controller/`, `tests/e2e/`
- accept: a virtual-Launchpad capture taken before and after arming differs in exactly one pad's `(velocity, mode)`; all other pads keep their patch colour; an erroring pad still flashes; `scripts/test.sh` passes.


## T202 · BUG-014 make the Grid page just the grid
- depends: none
- size: S
- goal: `/grid` renders the grid only. Remove `SystemPanels` from `GridRoute`; if the three panels are used nowhere else, delete them with their dead tests and fixtures instead of leaving unused components behind.
- files: `ui/src/features/shell/GridRoute.tsx`, `ui/src/features/shell/SystemPanels.tsx`, `ui/src/testing/`
- accept: `/grid` contains `instrument-grid` and no `app__panels`; no unused panel component remains; `pnpm typecheck`, `pnpm test:snapshot` and `pnpm format:check` pass.


## T203 · BUG-015 move Audio & MIDI to the footer
- depends: none
- size: S
- goal: Take Audio & MIDI out of the sidebar workspace and render it in the shell footer next to the status line; the sidebar keeps Grid, Instrument and Plugins. Keep the item in the narrow-layout bottom tabs so the route stays reachable.
- files: `ui/src/features/shell/navigation.ts`, `ui/src/features/shell/Sidebar.tsx`, `ui/src/features/shell/ShellFooter.tsx`, `ui/src/features/shell/BottomTabs.tsx`, `ui/src/testing/`
- accept: at 1200x800 `nav-audio` is inside `app-footer` and not inside `app-nav`, and clicking it opens `/audio`; at a narrow width the bottom tabs still show it; `pnpm typecheck` and `pnpm test:snapshot` pass.


## T188 · App settings store
- depends: T187
- size: M
- goal: Add `engine/src/settings/SettingsStore` for the file described in `plan/SETTINGS.md`: `<userApplicationDataDirectory>/Instrument Hub/settings.json`, schema `version: 1`, load with defaults, save atomically (temp file + rename in the same directory), preserve unknown keys on rewrite, rename a corrupt file to `settings.json.bad` and continue with defaults. No wiring into the app yet; the store is exercised only by its own tests.
- files: `engine/src/settings/`, `engine/CMakeLists.txt`, `tests/settings/`, `tests/CMakeLists.txt`
- accept: a new `ihub-settings-store-test` covers round-trip, defaults for a missing file, corrupt-file recovery, unknown-key preservation, atomic replace (no partial file after a simulated failure) and a schema with a newer version being rejected without touching the file; `scripts/test.sh` passes.
- note: use a temporary directory per test, never the real `~/.config/Instrument Hub`.


## T189 · Restore the audio device on launch
- depends: T188
- size: M
- goal: Wire the store into the audio path. `SessionSetup` restores the stored `(deviceType, deviceName, sampleRate, bufferSize)` when no `--audio-device`/`--audio-type` is given, and a successful `POST /audio` apply writes the effective values. A missing or unopenable device falls back to the device default, then the first ALSA output, without rewriting the stored preference, and `GET /audio` gains a `restore` object (`usedStoredDevice`, `fallback`, `reason`) so the UI can explain what happened.
- files: `engine/src/app/SessionSetup.cpp`, `engine/src/audio/settings/`, `engine/src/audio/AudioSettings.cpp`, `engine/src/api/routes/AudioRoutes.cpp`, `tests/api/audio_settings/`, `tests/settings/`
- accept: a regression opens a stored device, restarts the settings/session path against the same temp config dir and asserts the second open uses it; CLI flags beat the file; a stored device that no longer exists falls back and leaves the file byte-identical; a failed `POST /audio` writes nothing; `scripts/test.sh` passes.
- note: `BUG-006` evidence is the manual check — select the UR22, restart the real app, confirm `GET /audio` still reports it.


## T190 · Persist app-level settings
- depends: T188
- size: M
- goal: Route the app-level settings through the store: `masterGain`, `inputGain`, `midiChannel`, `metronome`, the LAN mode and the pairing token. Load on startup, save when `POST /settings` accepts a change and when LAN/token change, and write the file `0600`. Revoking the token deletes it from the file. Keep the API shape and ranges unchanged.
- files: `engine/src/api/state/`, `engine/src/api/AuthGuard.cpp`, `engine/src/settings/`, `tests/api/`
- accept: a regression changes each value, reconstructs the state store against the same temp config directory and asserts the values return; invalid values are rejected and not written; the settings file is `0600`; a revoked token is gone after a restart; `scripts/test.sh` passes.


## T191 · Persist enabled MIDI inputs by port name
- depends: T188
- size: M
- goal: Remember which MIDI inputs the user enabled. Persist `(client name, port name)` pairs, never the numeric `ALSA:<client>:<port>` id, and re-apply them when the ports are enumerated; a port that is absent stays in the file so it re-enables when the device returns.
- files: `engine/src/api/MidiBridge.cpp`, `engine/src/settings/`, `tests/api/midi/`, `tests/settings/`
- accept: a regression enables two synthetic ports, reconstructs the bridge, and asserts only those two are enabled; a stored port missing from the enumeration is skipped without erasing it; the Launchpad/Keystation role tests are unchanged; `scripts/test.sh` passes.


## T192 · Show restore fallback and offer to forget the device
- depends: T189
- size: S
- goal: Surface `restore` in the Audio page: when the stored device was not used, show the chosen fallback and the reason, with a "Forget device" action that clears the stored audio entry and a "Use this device" action that re-applies the stored one. No changes to the engine API beyond what T189 added.
- files: `ui/src/features/audio-settings/`, `ui/src/testing/`
- accept: a snapshot scenario renders the fallback notice and both actions; `pnpm typecheck` and `pnpm test:snapshot` pass; formatting is Prettier-clean.


## T193 · Card-based audio device list
- depends: T189
- size: L
- goal: Replace the raw ALSA PCM enumeration with the card-based list described in `plan/DEVICES.md`: one entry per card (`Steinberg UR22mkII`, `HD-Audio Generic (analog)`, `HD-Audio Generic (HDMI)`) plus `System (PipeWire)`; each entry resolves to a PCM family (`hw`, then `plughw`/`front`, then the card default); candidates that cannot be opened with JUCE's format set are omitted, and a busy card is listed once as unavailable with `in use by another process`. Hide `VirMIDI`, `Loopback`, MIDI cards and ALSA plugin PCMs; keep the raw list behind `--raw-devices` and the Loopback behind `--show-test-devices`, which the test rig and `scripts/test.sh` pass.
- files: `engine/src/audio/device/`, `engine/src/audio/AudioSettings.cpp`, `engine/src/app/SessionSetup.cpp`, `tests/device_manager/`, `tests/CMakeLists.txt`
- accept: `GET /audio` on this machine returns the UR22, the two onboard outputs and `System (PipeWire)` and nothing else; `--list-devices --raw-devices` still prints every PCM; the Loopback appears only with `--show-test-devices`; `ihub-device-manager-test` and the audio-settings tests pass unchanged.
- note: the UR22 exposes S32_LE only, so a float-only probe must fall through to S32 rather than reporting an empty device; this is the root cause of BUG-007's channel error.


## T194 · Make a device switch succeed in one request
- depends: T193
- size: M
- goal: Fix `AudioSettingsBridge::apply` for device-only patches: default output channels to the first two and inputs to all reported channels without requiring a probe, and only emit the channel error when the caller sent a channel list. Report `EBUSY` as "in use by another process (PipeWire)" and a missing card as "not connected". Switching to the UR22 must not require the UI to send channel masks.
- files: `engine/src/audio/settings/`, `engine/src/audio/device/`, `tests/api/audio_settings/`
- accept: `POST /audio` with only `deviceName` and `sampleRate`/`bufferSize` opens the device and returns the live latency; a busy device returns the busy message; the existing channel-selection tests are unchanged; `scripts/test.sh` passes.


## T195 · Device picker UI
- depends: T194
- size: M
- goal: Show the curated list in the Audio page with the card name and a short description, render unavailable devices disabled with their reason, add the Advanced toggle that reveals the raw PCM list, and surface the `restore` notice from T189 next to the picker.
- files: `ui/src/features/audio-settings/`, `ui/src/testing/`
- accept: a snapshot scenario shows four device entries and no duplicate card rows, an unavailable card renders disabled with its reason, the Advanced toggle reveals the raw list; `pnpm typecheck`, `pnpm test:snapshot` and `pnpm format:check` pass.


## T205 · Project lifecycle: new, open, save as, recent
- depends: T188, T199, T204
- size: L
- goal: Make projects first-class: `GET /project` returns path, name and dirty state; `POST /project` supports new, open, save-as and close, each writing through the autosave path; app settings keep the last opened path and a recent list; the footer shows the project name and dirty marker with Save and Open actions; `--project` overrides the stored path for that run only.
- files: `engine/src/api/`, `engine/src/app/`, `engine/src/settings/`, `ui/src/features/shell/`, `tests/api/project/`, `ui/src/testing/`
- accept: Playwright switches projects and the grid, instruments and mixer follow; a dirty project shows the marker and clears it after autosave; Save-as writes a new file and leaves the original untouched; the recent list survives a restart; `scripts/test.sh`, `pnpm typecheck` and `pnpm test:snapshot` pass.

## T206 · BUG-016 offer JACK devices in the audio picker
- depends: none
- size: M
- goal: Add non-ALSA devices to the curated `GET /audio` device list so the Audio page can select a JACK client, keeping ALSA the default and the raw ALSA PCM list unchanged. Enumerate each non-ALSA type's client names, describe them as `<TYPE> audio device`, and probe channel names, sample rates and buffer sizes through a temporary device without activating it; a type with no server contributes no devices.
- files: `engine/src/audio/device/`, `engine/src/audio/DeviceManager.h`, `tests/device_manager/`, `docs/architecture.md`
- accept: with no JACK server `GET /audio` still lists only ALSA cards and `ihub-device-manager-test` passes; with a running server each JACK client appears as a card and `POST /audio` opens it; `scripts/test.sh` passes.
- note: verified 2026-11-03 through PipeWire's JACK server (B012 resolved): the app lists 11 JACK cards and opens `Steinberg UR22mkII  Pro` at 48 kHz / 128 frames / 6.67 ms with no new xruns.

