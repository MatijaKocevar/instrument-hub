# Bugs — current phase

The feature roadmap (`T000–T139`) is complete and the app is playable:
the Launchpad selects patches, the Keystation plays the armed patch, and
per-patch Analog Lab state survives switching and restarts.

**This file is now the work queue.** The next phase is bug fixing, not new
features and not refactoring. A bug only enters this file when it is
reported by the human or reproduced on this machine.

## Rules

- One bug per entry, one fix per commit.
- Reproduce first: exact steps, expected, actual, and the log/screenshot.
- Add a regression test where practical; the fix is not done until
  `scripts/test.sh` passes.
- Keep the audio-thread contract and the Launchpad/Linux assumptions.
- Do not start speculative refactors or features while fixing bugs.

## Template

```
## BUG-000 · short title
- status: open | reproduced | fixed | verified
- area: grid | instrument | plugins | audio | launchpad | midi | engine | ui
- steps:
  1. ...
- expected:
- actual:
- evidence: log line, screenshot, test name
- fix: commit and what changed
```

## Open

### BUG-001 · `/audio` reports `open=false` while the session is streaming sound
- status: fixed
- area: audio
- steps: launch the app, read `GET /audio`
- expected: `open: true` for the device the session opened
- actual: `open: false`, `deviceName: PulseAudio Sound Server`; the session log says the device opened at 48 kHz/128
- evidence: `session: opened PulseAudio Sound Server...` in the launch log, `/audio` returns `open=false`; `ihub-audio-settings-test` now opens Loopback on an external manager, checks the un-injected bridge still reports closed, then injects the manager and asserts `open`, device name, rate and buffer follow it
- fix: T183 injects `session->getDevices()` into `AudioSettingsBridge`; the owned manager remains the `--serve`/`--audio` fallback and the latency probe uses its own manager so measuring never closes the live session device. Regression failed 4/4 injected checks before the wiring and passes after; `scripts/test.sh` passes 65/65.

### BUG-002 · Full 64-patch Analog Lab workflow is only partly automated
- status: fixed
- area: instrument
- steps: repeat the manual checklist `docs/manual-analog-lab-64.md`
- expected: all 64 Analog Lab patches restore their own preset across a full pad sweep
- actual: before T186 the automated coverage was 8 TestPlugin patches plus 2 real Analog Lab patches; loading 64 Analog Lab instances in one process stack-overflows the Wine bridge
- evidence: `ihub-multipatch-batch` covers all 64 TestPlugin patches in eight child processes of eight patches against one shared 64-slot sidecar; each patch runs through a T184 sandbox helper, accepts a distinct generated preset, is captured to the sidecar, is reloaded in a fresh session, and returns the same 16-hex state hash; the parent requires 64 sidecar files, 64 unique hashes and at most two live instances per batch; `ihub-per-patch-state` (8 patches) and `ihub-analog-lab-state` (2 real Analog Lab patches) still pass; `scripts/test.sh` -> 100% tests passed, 0 failed out of 68 (116.30 s)
- fix: T186 adds the batch regression under `tests/e2e/multipatch_batch` and writes `ihub-multipatch-batch-record.txt`. It reuses the T184 `SandboxHost` boundary and spawns one child per family, so no engine or bridge process ever accumulates all 64 plugin instances; no engine code changed. Remaining human work: the real Analog Lab V 64-pad audio, editor-follow and restart pass on a licensed machine, per `docs/manual-analog-lab-64.md` and B003; the automated TestPlugin batch proves state round-trips, not Analog Lab preset selection or sound.

### BUG-003 · Global page header and connection bar repeat on every page
- status: fixed
- area: ui
- steps: open any desktop route (`/grid`, `/instrument`, `/plugins`, `/audio`, `/settings`)
- expected: the page starts with its own content; app identity and connection state live in the sidebar/footer
- actual: every route prints the "Audio workspace / Instrument Hub / One place…" hero header and a full-width Connected / URL / Retry now bar
- evidence: screenshot in the bug report; `ui-shell-*` and page artifacts under `tests/artifacts/`; T140 harness asserts no `app__header` or `connection-status` on all five desktop routes; `ihub-ui-snapshot` captures the populated 1200×800 Grid
- fix: T140 removes the global hero header and connection bar, keeps the phase in the sidebar and footer, and probes `sidebar-title` / grid cells instead of `app-title`

### BUG-004 · App does not exit after the window is closed; hangs in shutdown
- status: fixed
- area: engine
- steps:
  1. start `build-release/engine/instrument-hub` in GUI mode and play with Analog Lab patches (log `/tmp/opencode/run-launchpad.log`)
  2. close the window a few minutes later
- expected: the window disappears and the process exits within a second, reaping the vstbridge host
- actual: window stays mapped but never repaints; the process keeps running; `127.0.0.1:8080` is no longer listening, so `shutdown()` already reached `controlApi.stopServer()`; the main thread sits in `futex_wait_queue`; one thread burns ~100% CPU; `WM_DELETE_WINDOW` is ignored; only `SIGTERM` ends it
- evidence: launched 2026-11-01 17:08, found stuck at 17:21 (PID 61425, 14 min elapsed, 8 threads, `/proc/61425/task/61425/wchan` = `futex_wait_queue`, thread 61428 at 99.9%); `ss -ltnp` shows no 8080 listener; `xwininfo` reports the window `IsViewable` at 3840+24; a hand-sent `WM_DELETE_WINDOW` (`/tmp/opencode/xclose 0x560000c`) was ignored; `kill -TERM 61425` exited immediately; `/tmp/opencode/run-launchpad.log` stops at 17:11:00; `ihub-shutdown-test` now builds a session with a warm pooled patch and a live control API, stops both, and requires the whole teardown in < 2 s (15.4 ms); with the dispatch loop already stopped it starts a sandbox plugin load whose helper blocks in startup and requires shutdown in < 2 s (3.1 ms, slot reports `sandbox startup cancelled`); `scripts/test.sh` -> 67/67 passed
- fix: T185 closes the shutdown path. `WarmPoolController::shutdown()` now cancels in-flight loads before draining, waits for the worker with a 1 s budget instead of `pumpMessages(2)` (which busy-spins once the quit message has been received), and abandons a worker that ignores the budget rather than joining it forever. `WarmPool::shutdown()` sets quit, joins only after the worker signals completion, and destroys the abandoned instances on the calling thread; the old destructor path could join a worker stuck in `SandboxHost::start()`. `SandboxHost` gained `shouldAbortStart`, polled every 2 ms, so a shutdown kills the helper instead of waiting out its 15 s startup timeout; `Session::beginShutdown()` sets that token and skips plugin-state capture while the session is tearing down. `Main::shutdown()` now calls it before `closeDevice()` and releases the audio bridge/server before the session. The original Analog Lab hang was not reproduced without the real plugin: the regression reproduces its shape (worker blocked in plugin startup, message loop already stopped) with a fake stalling sandbox helper. Remaining gap: an in-process editor mirror or a non-sandbox factory that blocks and ignores the token can still leave a worker behind; the controller abandons it instead of keeping the process alive, so any future blocking factory must honour the cancellation token before it can be considered shutdown-safe. The sandboxed audio path itself is unaffected.

### BUG-006 · Audio device and mixer settings are lost on every restart
- status: open
- area: audio
- steps:
  1. start the app and select the UR22mkII direct hardware device in `/audio` at 48000/128
  2. confirm `GET /audio` reports ~5 ms total latency
  3. quit and start the app again
- expected: the app reopens the UR22 directly and the latency readout stays low
- actual: it reopens `PulseAudio Sound Server` through PipeWire at 18.7 ms; master gain, MIDI channel and enabled MIDI inputs reset too
- evidence: 2026-11-03 running app reports `deviceName: PulseAudio Sound Server`, `totalMs: 18.67`; `~/.config/Instrument Hub/` holds only `KnownPluginList.xml` and `PluginCache.xml`, no settings file; `app/SessionSetup.cpp` falls back to the first ALSA output and `api/state/TransportState.h` is memory-only
- fix: pending; design is `plan/SETTINGS.md`, implementation T188–T192

### BUG-007 · Device picker lists ~60 ALSA PCMs and switching to the real interface fails
- status: open
- area: audio
- steps:
  1. open `/audio` and the device picker
  2. select `Steinberg UR22mkII, USB Audio; Direct hardware device without any conversions` and apply
- expected: one entry for the UR22, one for the onboard analog output, and the switch opening the UR22
- actual: 60 entries for two cards (dmix, dsnoop, surround, iec958, rate converters, Loopback, JACK, PipeWire); applying the UR22 returns `outputChannels must select at least one channel`; picking `HD-Audio Generic … Direct sample mixing device` opens a 66.7 ms device
- evidence: 2026-11-03 `GET /audio` returns 60 devices for cards `UR22mkII` and `Generic`; `POST /audio` with only `deviceName` answers the channel error; while the app played through PipeWire its sink was `RUNNING` and the UR22 hw PCM was unavailable, after the app moved to the onboard dmix PCM the same `hw:CARD=UR22mkII,DEV=0` opens with `aplay` (S32_LE only, float rejected)
- fix: pending; design is `plan/DEVICES.md`, implementation T193–T195

### BUG-005 · Engine aborts when the Wine plugin bridge dies
- status: fixed
- area: engine
- steps:
  1. start the app in development and arm an Analog Lab patch so its `vstbridge-host` is running (engine 450923, host 452420, 21:53)
  2. `kill -9` the `vstbridge-host` process
- expected: the slot reports a failed plugin and the engine keeps running; a hostile plugin that calls `abort()` already dies alone in `ihub-sandbox` (`tests/plugins/SandboxTest.cpp`)
- actual: the engine dies with SIGABRT in the same second; the control API goes away, and because Vite still serves `:5173` the UI loads but every request fails, which reads as "the dev server stopped working"
- evidence: two identical events, both at the second the host was killed — `20:16:12Z WATCHDOG killing stuck vstbridge-host pid=400966` with `/tmp/instrument-hub-crash.log` rewritten at 21:16:12 local, and `20:55:20Z ... pid=452420` with the dump rewritten at 21:55:20 (`signal: 6 SIGABRT`, `records: 0`); only `engine/src/Main.cpp` installs the dump handler, so the abort was the main process, not `ihub-sandbox` and not Wine; after 21:55 no `build-debug` process remained. `ihub-session-sandbox-death` now warms a TestPlugin patch in a helper process, confirms `getSandboxProcessId(0) > 0`, SIGKILLs that helper while a render thread runs, then asserts `isSlotPluginFailed(0)` and a non-empty `getSlotError(0)` while the other slot keeps rendering (RMS 0.09). `scripts/test.sh` -> 66/66 passed.
- fix: T184 routes every plugin slot through `plugins::SandboxHost` in `SessionBuild.cpp`; audio and MIDI cross the existing shared-memory rings, plugin state is applied and captured over a new shared-memory control command, and a dead helper becomes a slot error instead of an in-process abort. Editor and parameter access use a lazily created in-process `PluginInstrument` mirror, so the sandboxed audio path never loads the plugin in the engine process. Remaining gap: while an editor mirror is open that second instance is still an in-process crash surface, and a yabridge bundle that cannot host a second instance concurrently can stall the mirror load (the editor API fixture now prefers vstbridge, the production bridge from this bug); the sandboxed audio path itself stays isolated.
