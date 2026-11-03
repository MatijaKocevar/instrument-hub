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

### BUG-015 · Audio & MIDI is a sidebar workspace item, not a footer entry
- status: open
- area: ui
- steps: open any desktop route
- expected: the sidebar lists Grid, Instrument and Plugins; Audio & MIDI sits in the footer with the status line
- actual: `WORKSPACE_ITEMS` puts Audio & MIDI fourth in the sidebar
- evidence: `ui/src/features/shell/navigation.ts:14`, `Sidebar.tsx:39`
- fix: pending; T203

### BUG-014 · Grid page opens with three engine cards above the grid
- status: open
- area: ui
- steps: open `/grid`
- expected: the page is the 8×8 grid only
- actual: `EnginePanel`, `TransportPanel` and `SettingsPanel` render as a three-card band above it
- evidence: `ui/src/features/shell/GridRoute.tsx:34` renders `SystemPanels`, which renders the three panels
- fix: pending; T202

### BUG-013 · Arming one pad recolours the other Launchpad pads
- status: open
- area: launchpad
- steps: arm a patch from the controller and watch the grid LEDs
- expected: only the armed pad changes (to red); every other pad keeps its patch colour
- actual: other pads change colour as their warm state flips (`warming` paints every pad yellow, `error` red flashing)
- evidence: `engine/src/app/ControllerSurface.cpp:147-151` derives the LED from the live warm state for all 64 pads; `LaunchpadLed.cpp:24-37` lets `warming`/`error` override the patch colour
- fix: pending; T201

### BUG-012 · Changing a sound in Analog Lab silences audio until the editor is closed
- status: open
- area: instrument
- steps:
  1. arm an Analog Lab patch and confirm it plays
  2. open the plug-in editor and change the preset
  3. play the Keystation
- expected: the new sound plays immediately
- actual: audio is silent for about a second and stays silent until the editor window is closed, then the new sound plays
- evidence: reported 2026-11-03; root cause not yet captured — the sound reaches the sandboxed audio instance only through the editor mirror, which T184 introduced
- fix: pending; T200 reproduces first and records the log

### BUG-011 · Patch edits are never written to the set file
- status: open
- area: engine
- steps: change volume, pan, colour or source on the Instrument page, then restart the app
- expected: the values return
- actual: every edit is lost; the set file on disk is unchanged
- evidence: `ihub::project::Serializer::save` has no caller outside `tests/project/SetFileTest.cpp`; `StateStorePatches` mutates memory only
- fix: pending; T199 makes the `.ihub` project the live set, T204 autosaves it

### BUG-010 · The grid's armed colour lags the Launchpad by up to two seconds
- status: open
- area: ui
- steps: arm a patch from the Launchpad while looking at `/grid`
- expected: the red cell follows the pad immediately
- actual: the pad lights at once, the UI catches up on the next poll
- evidence: `ui/src/state/options.ts:18` sets `pollIntervalMs: 2000`; state reaches the UI only through `PollLoop`, the WebSocket carries meters only
- fix: pending; T198

### BUG-009 · The patch palette starts with red, so a whole row is red
- status: fixed
- area: ui
- steps: open `/grid`
- expected: no red except the armed patch
- actual: colour index 0 is `#f43f5e` in the UI and Launchpad velocity 5 (red) on the controller, so index-0 rows read as a red row; the rose at index 13 does the same
- evidence: `ui/src/features/grid/colours.ts:2-17` now runs from `#fb923c` through `#f472b6` to slate, with no hue in the red band; empty cells use `EMPTY_COLOUR` `#64748b`; `ihub-launchpad-protocol` -> `ok  : no patch palette velocity is the armed red (16-index sweep)`; `build/tests/ihub-e2e-pages-record.txt` -> `armed_led_velocity=5`, `idle_led_velocity=17`, `red_led_count=1`, `red_pad=10`, `status=PASS`; `cd ui && pnpm test:snapshot` -> `PASS no patch palette colour has a red hue`, `PASS empty cells use the neutral fill`, `PASS empty cells use the neutral gradient`, `ui-snapshot: all checks passed`; `scripts/test.sh` -> 100% tests passed, 0 failed out of 68
- fix: T197 shifts the Launchpad patch velocities to `9,13,17,21,25,29,33,37,41,45,49,53,56,57,3,1` so velocity 5 stays reserved for armed/error, replaces the rose UI colours with the same non-red order, and paints empty cells with a neutral slate gradient.

### BUG-008 · Grid rows are mirrored between the UI and the Launchpad
- status: fixed
- area: grid
- steps: open `/grid` at 1200x800 and compare the first family row with the bottom-left row of the Launchpad
- expected: the same slots in the same physical place
- actual: slot 1 is the top-left cell on screen but the bottom-left pad (note 11) on the controller, so the first row reads red at the top on screen and at the bottom on the hardware
- evidence: Playwright before the fix read `data-row="0"` first with `data-slot` 0–7; after the fix the first rendered family row is `data-row="7"` with `data-slot` 56–63 and the last is `data-row="0"` with 0–7 (screenshots `tests/artifacts/ui-grid-before-T196.png`, `ui-grid-after-T196.png`); clicking the top row's slot 60 opened `/instrument?slot=60` and `GET /state` reported `armedSlot: 60`, clicking the bottom row's slot 3 reported `armedSlot: 3`; `ihub-launchpad-protocol-test` -> `ok : note 11 maps back to pad 0`, `ihub-launchpad-driver-test` captures the note-11 LED as pad 0, `ihub-e2e` -> `pad 0 arms slot 0` and `PASS`; `cd ui && pnpm test:snapshot` -> `ui-snapshot: all checks passed` including `PASS the top rendered row holds slots 57–64`; `scripts/test.sh` -> 100% tests passed, 0 failed out of 68
- fix: T196 adds `gridRows()` in `ui/src/features/grid/model.ts`, which reverses the family order for rendering only; `GridTable` renders top-down while slot indexes, column order and all slot-keyed callbacks stay unchanged, and `data-row` keeps the hardware row (0 = bottom), so the rendered rows are numbered 08 at the top to 01 at the bottom. Snapshot assertions moved to `ui/src/testing/scenarios/gridSnapshot.tsx` with new top-down row checks against the 64-slot fixture.

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
