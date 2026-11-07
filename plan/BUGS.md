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
- status: fixed
- area: ui
- steps: open any desktop route
- expected: the sidebar lists Grid, Instrument and Plugins; Audio & MIDI sits in the footer with the status line
- actual: `WORKSPACE_ITEMS` puts Audio & MIDI fourth in the sidebar
- evidence: `ui/src/features/shell/navigation.ts:14`, `Sidebar.tsx:39`
- fix: T203 splits `AUDIO_ITEM` out of `WORKSPACE_ITEMS` and renders it in `ShellFooter` next to the status line with a footer variant of `NavLink`; the sidebar keeps Grid, Instrument and Plugins while the narrow bottom tabs still list all four workspace destinations plus Settings. `tests/artifacts/ui-shell-1200x800.html` has three `app-nav` items and `nav-audio` inside `app-footer` with `href="/audio"`, `tests/artifacts/ui-shell-narrow.html` keeps `nav-audio` in `app-bottom-tabs`; `corepack pnpm typecheck` -> clean; `corepack pnpm test:snapshot` -> all checks passed; `corepack pnpm format:check` -> all matched files use Prettier code style.

### BUG-014 · Grid page opens with three engine cards above the grid
- status: fixed
- area: ui
- steps: open `/grid`
- expected: the page is the 8×8 grid only
- actual: `EnginePanel`, `TransportPanel` and `SettingsPanel` render as a three-card band above it
- evidence: `ui/src/features/shell/GridRoute.tsx:34` renders `SystemPanels`, which renders the three panels
- fix: T202 removes `SystemPanels` from `GridRoute` and deletes `SystemPanels`, `EnginePanel`, `TransportPanel` and `SettingsPanel` with their dead `.app__panels`/`.panel` styles and the snapshot checks for the armed-slot and transport widgets. `tests/artifacts/ui-state-snapshot.html` now has one `instrument-grid` and no `app__panels`; `corepack pnpm typecheck` -> clean; `corepack pnpm test:snapshot` -> all checks passed; `corepack pnpm format:check` -> all matched files use Prettier code style.

### BUG-013 · Arming one pad recolours the other Launchpad pads
- status: fixed
- area: launchpad
- steps: arm a patch from the controller and watch the grid LEDs
- expected: only the armed pad changes (to red); every other pad keeps its patch colour
- actual: other pads change colour as their warm state flips (`warming` paints every pad yellow, `error` red flashing)
- evidence: `ihub-launchpad-driver` -> `PASS: 0 failed check(s)` with `arming one pad changes exactly one LED (pad 42)`, `the armed pad keeps a static red LED`, `arming sends exactly one LED frame, for the armed pad only`, `every other pad keeps its patch colour`, `a cold armed pad pulses red without touching the other pads`, `background warming does not repaint another pad`, `a non-armed warming pad keeps its static patch colour`, `a non-armed warm-state flip sends no LED frame`, `an erroring pad still flashes red` and `the error frame flashes red on the failing pad only`; `ctest --test-dir build -R '^ihub-e2e$'` -> Passed; `scripts/test.sh` -> 100% tests passed, 0 failed out of 69 (124.12 s)
- fix: T201 keeps the patch colour as the base LED and applies warm state only to the armed pad: it pulses red while loading and settles to static red, while background warm-pool preloads of other pads no longer repaint them. `launchpadLedForPatch` no longer paints warming yellow, `ControllerSurface::refreshLeds` passes warming only for the armed pad, and the e2e grid capture mirrors that policy. Errors still flash red on the failing pad.

### BUG-012 · Changing a sound in Analog Lab silences audio until the editor is closed
- status: fixed
- area: instrument
- steps:
  1. arm an Analog Lab patch and confirm it plays
  2. open the plug-in editor and change the preset
  3. play the Keystation
- expected: the new sound plays immediately
- actual: audio is silent for about a second and stays silent until the editor window is closed, then the new sound plays
- evidence: reproduced 2026-11-03 with Analog Lab V through vstbridge and `t200_analog_lab_editor_probe`: before the fix one `Master` edit blocked the message thread for 412.689 ms and 32 rapid editor edits took 8770.5 ms, because every parameter notification serialised the whole mirror and synchronously applied it to the sandbox; the sandbox loop was busy applying 100 KB states instead of rendering. After the fix the same edit took 0.135 ms and the 32-edit burst 67.7 ms (mostly the probe's own 2 ms sleeps), and the slot rendered -25.1 / -25.5 / -26.9 dBFS before, during and after the burst. `ihub-session-editor-follow` now checks the TestPlugin mirror with the editor open: 64 edits complete in 0.25-0.4 ms, the slot stays above -40 dBFS, the next render is 880.86 Hz, a preset-state change reaches the sandbox, the sandbox pid is unchanged and the played-block counter keeps rising; `ihub-per-patch-state` passes its 8-patch exact-state round trip; `ihub-sandbox`, `ihub-plugin-instance` and `ihub-session-plugin-state` pass. An earlier full `scripts/test.sh` run could not finish because the session's display stack had degraded (`sddm` at ~21 GB RSS, Xorg in an `accept() failed` loop); after it recovered, the full suite on the completed tree passes: 100% tests passed, 0 failed out of 69 (129.82 s), including the WebKit snapshot, GUI selftest, `ihub-session-editor-follow`, `ihub-per-patch-state` and `ihub-sandbox`. Remaining human step: the real Analog Lab editor preset-change check on a licensed machine.
- fix: T200 stops treating every editor parameter notification as a full plugin-state save. `PluginInstrument` now exposes a parameter-changed callback in addition to the state-changed callback, and `SandboxHost`/`ihub-sandbox` gained a bulk `applyParameters` control command keyed by parameter ID. `session::MirrorSync` coalesces edits per slot on a worker thread (10 ms quiet window, 64-parameter batches) and pushes only the changed parameters, so the editor/message thread never waits for the sandbox. Full state blobs are captured on the message thread and applied after a 150 ms quiet window, which covers preset loads without a per-parameter state storm. State captures and applies suppress re-entrant notifications, mirror state is now captured from the mirror rather than the sandbox when a sandbox is evicted, and the play path, audio device and slot graph are untouched.

### BUG-011 · Patch edits are never written to the set file
- status: fixed
- area: engine
- steps: change volume, pan, colour or source on the Instrument page, then restart the app
- expected: the values return
- actual: before the fix every edit was lost; the set file on disk was unchanged
- evidence: `./build/tests/ihub-project-test` -> `PASS: 0 failed check(s)` with `ten fader moves produce one file write (got 1)`, `the edited fader value reached the project file`, `the .bak is byte-identical to the previous project`, `a failed write leaves the previous .ihub byte-identical`, `a crash mid-write still leaves a complete .ihub` and `a read-only project keeps running and reports not saved`; `./build/tests/ihub-patch-state-capture-api-test` -> `all checks passed` with `the patch mixer and colour reached the project file`, `a restart loads the autosaved project`, `the FX chain reached the project file`, `the transport tempo reached the project file`, `the project stores the sound captured before the write` and `restoring app settings never writes the project`; `scripts/test.sh` -> 100% tests passed, 0 failed out of 69 (131.54 s)
- fix: T204 wires accepted patch/source/FX/transport/capture edits into `ProjectAutosave`: a 500 ms debounce, capture of the touched plug-in slots immediately before the write, `<name>.ihub.tmp` + atomic rename with `<name>.ihub.bak`, `project.dirty`/`saved`/`error` in `/state`, and a `project` state event. Failed or read-only writes leave the previous file intact and report the error.

### BUG-010 · The grid's armed colour lags the Launchpad by up to two seconds
- status: fixed
- area: ui
- steps: arm a patch from the Launchpad while looking at `/grid`
- expected: the red cell follows the pad immediately
- actual: before the fix the pad lit at once and the UI caught up on the next 2 s poll
- evidence: `/state` now carries a monotonic `revision`; `/events` frames carry it plus the full state, and the store applies any newer revision and ignores stale poll responses. `./build/tests/ihub-state-broadcast-test` -> `PASS: state broadcast acceptance (0 failure(s))`, with `the UI arm event arrived in 5.90 ms` and passing warm/device/pad revision checks; a real `instrument-hub --serve --api-port 18080` run with a Node WebSocket client measured `push armed=6 revision=1793669494497745 in 46.5 ms` and `poll armed=2 revision=1793669494497746 in 5.1 ms` after the socket closed; `cd ui && corepack pnpm test:snapshot` -> `ui-snapshot: all checks passed` including `the pushed state is applied without another GET /state`, `a stale frame cannot roll the store back`, `polling applies the external arm within three seconds`; `scripts/test.sh` -> 100% tests passed, 0 failed out of 68 (157.49 s)
- fix: T198 adds `StateStore::getRevision()` and `notifyChanged()`, includes `revision` in `/state` and `/events`, publishes warm/device/midi/pairing changes, and wires the UI store to the existing `/events` socket via `StateTransport`/`parseStatePushFrame`. Polling and its backoff are unchanged and remain the fallback; Vite proxies the existing `/events` endpoint so the dev grid uses it too.

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

### BUG-027 · Project lifecycle expects empty tracks to be warm
- status: fixed
- area: project | session
- steps: run `./build-tests/tests/ihub-project-lifecycle-api-test`
- expected: after `waitForWarmIdle`, an empty track project reports its eight slots as cold
- actual: the test expected eight warm slots, so it failed after BUG-020 stopped warming empty tracks; no engine defect
- evidence: 2026-11-05 the new project has `warmSlots` 0-7 but every slot is `kind: empty`, and `ihub-project-lifecycle-api-test` now checks `warmSlots == 0` -> `PASS: 0 failed check(s)`
- fix: the lifecycle test now asserts `GET /state keeps the eight empty track slots cold`, matching BUG-020; no engine change

### BUG-026 · Every track pad reads red in the track-grid e2e
- status: fixed
- area: launchpad, ui
- steps:
  1. run `ihub-e2e-test` on the current tree
  2. inspect the track-grid LED checks
- expected: one red selected track pad, the other seven keep their RGB instrument colours, and the API volume step lights its pad green
- actual: `exactly one track pad is red (8 found)`; the unselected track colour and API-volume checks also fail. The e2e still renders audio and switches all eight tracks correctly.
- evidence: 2026-11-05 `ihub-e2e-test` -> `FAIL: 3 failed check(s)`; the same three checks fail with `IHUB_SHARED_PLUGIN_HOST=0`, so this is independent of T313. The snapshot passed to the LED painter has `armedSlot=0` and only track 0 selected, but `paintTrackLeds()` calls `driver.setPadRgb()` for the other tracks while the test reads `driver.getPadLed()`; `setPadRgb()` writes `padRgb` and does not update the `padLeds` snapshot that `getPadLed()` returns. The uncommitted track-LED work (`TrackSurface.cpp`, `TrackPadLeds.cpp`) is the likely source.
- fix: `setPadRgb()` now stores the matching track-palette velocity in `padLeds` for `getPadLed()`/snapshots while still sending the RGB SysEx frame, and `paintTrackLeds()` passes `launchpadLedForTrackPad(...).velocity`. The track-pad fixture assigns an init-tone source to every track so the FX/volume/LED paths run, and the volume shades are 0x10/0x13/0x18/0x20 for steps 0-4. `ihub-launchpad-track-test` and `ihub-e2e-test` now pass; full suite 76 passed, 1 skipped, 0 failed.

### BUG-025 · Engine SIGTERM leaves a Wine launcher behind
- status: fixed
- area: plugins/sandbox, engine shutdown
- steps:
  1. run the shared-page build with eight Analog Lab V tracks and let the page reach `ready`
  2. `kill -TERM` the engine process
- expected: the page helper, the vstbridge group host and its Wine launcher all exit
- actual: the helper exits because it is now set to die with its parent (`PR_SET_PDEATHSIG`); the group host exits too and becomes a zombie, but its `start.exe` launcher (about 17 MB) survives under init. The engine had no SIGTERM handler, so `PageHost::shutdown` and the process-tree killer never ran on this path.
- evidence: 2026-11-05, shared page `ready` with pid 415531, helper 415549 and group host 415583; after `kill -TERM 415531`, the helper disappeared within 250 ms and the group host became `[vstbridge-host.] <defunct>`, while `start.exe /exec .../vstbridge-host.exe.so group` remained with ppid 1850 and 17.2 MB RSS. 2026-11-07 22:01 final debug build with the same eight-track project: `kill -TERM` the engine (pid 314031) removed the engine, helper 314367, `start.exe` 314371, vstbridge group 314402, `wineserver -p` 314069 and both `winedevice.exe` 314087/314166 in 781 ms; an earlier run took 756 ms. The only surviving process with a matching name is the pre-existing `vstbridge-host.` zombie 128993 (state `Z`, ppid 1535/systemd) that was already defunct before this run.
- fix: `app/TerminationSignal` installs a `SIGTERM`/`SIGINT` handler that only writes a byte to a non-blocking self-pipe, and a monitor thread turns that into `JUCEApplicationBase::systemRequestedQuit()`. The normal JUCE shutdown path then runs `PageHost::shutdown` and `stopProcess`, so the helper and Wine tree are reaped instead of being orphaned. `stopProcess` now runs the Wine-prefix stop (`wineserver -k` plus a direct `WINEPREFIX` sweep) before waiting on the vstbridge group, and shutdown uses a short helper timeout, so the whole tree is gone in under a second rather than after the previous six-second group wait. In-flight page control commands also stop waiting when shutdown begins.

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
- fix: in progress; T188 adds the settings store and T189 restores the stored device at launch, saves the effective device after a successful `POST /audio`, and reports `restore.usedStoredDevice`/`restore.fallback`/`restore.reason` in `GET /audio`; mixer and MIDI inputs remain T190–T191, and the manual restart check is still required.

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

### BUG-016 · JACK never appears as a selectable device
- status: verified
- area: audio
- steps:
  1. launch the app and open `/audio`
  2. look for a JACK output device next to the ALSA cards
- expected: `JACK` appears as a device card and can be selected, as T187's goal states
- actual: `deviceTypes` lists `JACK`, but `devices` holds only ALSA cards, the Advanced list is ALSA PCMs only, and `POST /audio` with any JACK name answers "the device is not connected"; JACK cannot be selected anywhere
- evidence: 2026-11-03 released app `GET /audio` -> `deviceTypes: ["ALSA","JACK"]`, `devices: [HD-Audio Generic (analog), Steinberg UR22mkII, System (PipeWire)]`, all 52 `rawDevices` typed `ALSA`; `--list-devices` -> `audio device types: ALSA, JACK` with only ALSA devices below it. `DeviceCardList.cpp` and `DeviceRawList.cpp` enumerate ALSA only, and `AudioSettingsBridge::apply` resolves names through those lists. This machine also has no JACK server (`jackd` not installed, no JACK socket), so the live server path is queued in `BLOCKERS.md` B012.
- fix: T206 appends non-ALSA device types to the curated list: each JACK client becomes a `<TYPE> audio device` card with channel names and rates probed through a temporary device, while a server-less type contributes no entries. `ihub-device-manager-test` now also asserts the list holds no JACK entry without a server.
- verified: 2026-11-03 with `pipewire-jack` installed and the app launched through `scripts/run.sh` (a `pw-jack` wrapper). `GET /audio` lists 11 `JACK` cards including `Steinberg UR22mkII  Pro`; `POST /audio` switched to that card and the app reports `open: true`, 48 kHz / 128 frames, 6.67 ms, 0.33% load and no new xruns over 12 s; `pw-link` shows the `JUCEJack` client connected to the UR22 playback ports. The requested buffer size is set as PipeWire's `clock.force-quantum` when the JACK device opens and removed when it closes (verified by switching back to ALSA and by closing the window), so the desktop default returns; a live buffer change reports "restart the app to change the JACK buffer size".

### BUG-017 · New projects open the preset Starter Set instead of a blank grid
- status: fixed
- area: engine
- steps:
  1. launch with no project, or issue `POST /project {"action":"new"}`
  2. look at the grid
- expected: an 8×8 grid with no instruments, plugins, patch names or preset colours
- actual: the app imported `shared/set.yaml`, producing named patches and Analog Lab on slots 0/48/56; every new project inherited the Starter Set
- evidence: 2026-11-03 `POST /project {"action":"new","name":"Untitled"}` created `Untitled 2.ihub`; `GET /state` returns families `row1`..`row8`, 64 patches with empty names and `colorIndex: 14`, and slot 0 armed with no source. `ihub-project` -> `PASS: 0 failed check(s)`; `ihub-project-lifecycle-api`, `ihub-launchpad-protocol`, `ihub-launchpad-mapping` and `ihub-launchpad-driver` pass.
- fix: `shared/blank-set.yaml` defines eight empty rows, `writeBundledProject` prefers it when creating a project, and an instrument may have an empty display name so blank patches render without text. The Starter Set stays available through Open or `INSTRUMENT_HUB_SET`.

### BUG-018 · Launchpad rows use near-identical colours and a salmon row that reads as red
- status: fixed
- area: launchpad
- steps:
  1. start the app with the Starter Set and look at the 8×8 Launchpad
- expected: each family row is a distinct hue, and only the armed pad is red
- actual: the palette stepped through `9,13,17,21,25,29,33,37`, so the bottom row was salmon/red-orange and five rows were green/teal shades; slot 0's `colorIndex: 1` override also made the bottom-left pad differ from its row
- evidence: 2026-11-03 photo of the Launchpad shows the salmon bottom row and the miscoloured bottom-left pad. The palette is now `13,21,29,37,45,49,3,1,17,25,33,41,48,8,2,12` (yellow/green/teal/blue/purple/white/grey, no red or near-red), the piano override is removed, and the UI palette follows the same order. `ihub-launchpad-protocol`, `ihub-launchpad-driver` and `ihub-launchpad-mapping` pass; `pnpm test:snapshot`, `pnpm typecheck` and `pnpm format:check` pass.
- fix: the new palette replaces the contiguous ramp with widely spaced hues, removes the patch-level colour override from the Starter Set, and aligns the UI palette with the Launchpad order.

### BUG-019 · Opening a Wine-hosted plugin editor aborts the engine
- status: fixed (containment); process isolation still missing
- area: plugins/editor
- steps:
  1. start the app with an Analog Lab V patch (vstbridge/Wine) armed
  2. open the Instrument page and press "Open editor"
- expected: the plugin editor opens; if the Wine host dies the slot reports an error and the engine keeps running
- actual: the editor mirror loads a second vstbridge instance in the engine process; when the Wine host dies, vstbridge's asio reader throws `std::system_error: write: Broken pipe` on a background thread, nothing catches it, and `std::terminate` aborts the whole app
- evidence: 2026-11-04 20:28:39 dev build, `POST /patches/0/editor`; `/tmp/opencode/dev.log` ends with `terminate called after throwing an instance of 'std::system_error'`, `what(): write: Broken pipe` and `Aborted (core dumped)`. GDB on `build-debug/engine/instrument-hub` stopped in `std::terminate` with the throwing thread in `asio::detail::do_throw_error` / `asio::read<...local::stream_protocol...>` inside `libvstbridge-vst3.so`; the Wine log immediately before it says `WARNING: The native plugin host seems to have died`. This is the remaining gap recorded in BUG-005: the editor mirror is still an in-process crash surface.
- fix: `SessionPluginMirror` refuses to create an in-process mirror when the plugin bundle path contains `vstbridge`, so the editor API returns "the editor is unavailable for Wine-hosted plugins: the vstbridge host aborts the engine" instead of aborting the engine. Native plugins and the TestPlugin editor path are unchanged.
- remaining: host the editor in the sandbox process (the instance that already owns the audio) or in a dedicated editor helper, so a dead Wine host only fails that process. The sandbox currently links the headless processor module, which cannot create editors.

### BUG-020 · Empty track slots are warmed
- status: fixed
- area: session/warm pool
- steps:
  1. open a blank eight-track project (`shared/tracks-set.yaml`) with no instruments assigned
  2. read `GET /state`
- expected: unassigned tracks stay cold; only assigned tracks are warmed
- actual: all eight track slots show `warmState: "warm"` even though every slot is `kind: empty`, because `SessionBuild` gives every track a warm request and `preloadAll` warms every protected slot
- evidence: 2026-11-04, `track-check.ihub` with only Track 1 assigned, `GET /state` reported `warmState: "warm"` for every unassigned track slot; `SessionBuild.cpp` builds a request for every slot and `ProjectSession.cpp` puts all eight track slots in `warmSlots`
- fix: for the tracks layout, `SessionBuild` clears the warm request for `SessionSlotKind::empty` slots and `Session::setSlotSource` clears it when a track is assigned back to empty, so `preloadAll` skips them. Assigning an instrument sets a valid request again and the slot warms normally.
- verified: 2026-11-04 debug build with `track-check.ihub` (Tracks 1-3 assigned, 4-8 empty): `GET /state` reports `warmState: "warm"`/`active` for slots 0-2 and `cold` for slots 3-7.

### BUG-021 · Clicking a track pad opens the instrument instead of arming
- status: fixed
- area: UI/track grid
- steps:
  1. open the Grid page in a tracks project
  2. click a track cell in row 08 with the mouse
- expected: a single click arms that track (same as pressing its Launchpad pad) and the grid stays visible; a double click opens the Instrument page for it
- actual: `GridRoute.selectTrack` called `router.navigate("/instrument")` immediately, so the grid disappeared. In dev the arm also failed because `ui/vite.config.ts` did not proxy `/tracks` to the engine, so the `POST /tracks/{index}/select` went to Vite and rolled back
- evidence: 2026-11-04 reported on the dev app; Playwright on `http://localhost:5173/` showed a single click leaving `data-armed-slot` unchanged, and the old route jumped to `/instrument`
- fix: single click calls `selectTrack` only; double click calls `openTrack` (`selectTrack` plus navigation to `/instrument`). `ui/vite.config.ts` now proxies `/tracks`, `/project` and `/settings` to `127.0.0.1:8080`, so arm/new/save/settings calls work in dev.
- verified: Playwright single click on Track 3 -> URL stayed `/`, `data-armed-slot` became `2`; double click on Track 3 -> URL became `/instrument`; `GET /state` reported `armedSlot: 2` and Track 3 selected.

### BUG-022 · Browser UI routes return API JSON on reload
- status: fixed
- area: UI/dev server
- steps:
  1. run the UI through Vite (`http://localhost:5173/`) and open the Audio & MIDI page
  2. reload the page, or open `http://localhost:5173/audio` directly
- expected: the Audio & MIDI page renders with its device controls
- actual: `/audio`, `/plugins` and `/settings` are both UI routes and API endpoints, and the Vite proxy forwarded the browser navigation to the engine, so the tab showed raw JSON instead of the app
- evidence: 2026-11-04, `curl -H 'Accept: text/html' http://localhost:5173/audio` returned the engine's `AudioState` JSON; client-side clicks worked, reloads and direct links did not
- fix: a Vite `configureServer` middleware rewrites HTML navigations for `/audio`, `/plugins` and `/settings` to `/` before the proxy runs, while API fetches (`Accept: application/json`/`*/*`) still go to `127.0.0.1:8080`. The proxy now also forwards `/tracks`, `/project` and `/settings`.
- verified: `Accept: text/html /audio` returns `index.html`; `Accept: application/json /audio` returns the device state; Playwright loads `/audio` directly and finds `audio-page` with 3 device cards.

### BUG-023 · Arming an empty track leaves the previous instrument playing
- status: fixed
- area: session/tracks
- steps:
  1. open a tracks project with instruments on Tracks 1–3 and empty instruments on Tracks 4–8 (`/tmp/opencode/track-check.ihub`)
  2. arm Track 4, which has no instrument, from the Launchpad or the Grid
- expected: the previous instrument stops and the armed empty track produces silence
- actual: `armedSlot` changes to 3 and Track 4 is selected, but slot 2 remains active and its Analog Lab keeps sounding; slot 3 stays `warming`/`idle` instead of becoming active
- evidence: 2026-11-04 running `--serve` with `track-check.ihub`; after arming Track 4, `GET /state` reported `armedSlot: 3`, `slots[2] = {warmState:"active", state:"active"}` and `slots[3] = {warmState:"warming", state:"idle"}`. `WarmPoolController::arm` only called `finishArmLocked` from `warmCallback`; an empty track has no warm request, so `SlotSwitcher::armSlot(3)` was never called and the switcher kept rendering slot 2. `ihub-session-warm-protected` now builds a tracks session with instruments on slots 0–2 and empty slots 3–7, arms slot 2, then arms empty slot 3 and checks: requested slot 3, `warmState(3) == "active"`, switcher armed slot 3, slot 2 no longer active, post-fade output RMS < 1e-6, slot 3 gain > 0.99, slot 2 gain <= 0 and slot 2 state `idle`; `ctest -R '^ihub-session-warm-protected$'` -> 1/1 passed.
- fix: `WarmPoolController::arm` completes the arm directly when the target slot has no warm request: `finishArmLocked(slot, nullptr)` clears the voice, arms the switcher, clears `pendingArmSlot`, records the slot as active and keeps the previous slot pinned for the fade-out. `finishArmLocked` now accepts a null instance for empty slots.

### BUG-024 · Eighth Analog Lab track is silent; its sandbox deadlocks in applyState
- status: fixed
- area: plugins/sandbox, session/warm pool
- steps:
  1. open `/tmp/opencode/track-check.ihub` (eight tracks, each hosting Analog Lab V through vstbridge/Wine)
  2. let all eight slots warm, then arm Track 8 (`slot 7`) from the Grid or `POST /arm {"slot":7}`
- expected: Track 8 sounds and `GET /state` reports `warm: true`, `state: "active"`, `error: ""`
- actual: before the fix the slot became active but produced no audio; after a few seconds `GET /state` reported `error: "sandbox stopped responding"`. Seven `vstbridge-host` processes remained; the eighth was killed by the watchdog. A healthy slot (Track 1) produced a meter peak around 0.21 with the same MIDI note, Track 8 stayed at 0.
- evidence: 2026-11-05 debug build, Windows-style MIDI note sent through VirMIDI to the armed slot. The failed slot's shared memory (`/tmp/ihub-sandbox-*.ihubshm`) showed `requestPublished=8`, `requestConsumed=0`, `replyPublished=0` and `control = { command: 0, status: 1 (pending), stateBytes: 102891 }`: the Wine host never consumed the `applyState` command. All eight `plugin-state/slot-*.bin` files in the project were byte-identical, so the state contents were not the difference. Raising `defaultControlTimeoutMs` 3000 -> 15000 and `hangTimeoutMs` 5000 -> 20000 delayed the failure but did not fix it. After T313 the same project reports one `ihub-sandbox` page host and one `vstbridge-host.exe.so group` process; all eight slots are `warm` with empty errors; arming all eight performs no load/warming (`ihub-session-concurrent-instances-test`). `kill -9` of the group host reported `recovering` on the next poll, muted the page, restarted the host, restored all eight committed states and returned `ready` with `activeSlot: 7`; the old host tree was reaped. Manual `POST /pages/tracks/restart` returns through the same path. `ihub-sandbox-test` covers a 3-slot page and crash/manual recovery; `ihub-session-concurrent-instances-test` covers the legacy per-slot path and the 8-slot shared page. Memory on this machine with the same eight Analog Lab V tracks: eight separate hosts = 9,962 MB (8 × 1.15-1.19 GB vstbridge + 8 × 20-26 MB helpers + 8 × 17 MB start.exe + engine/WebKit) at ~211% CPU; one shared host = 2,187 MB (1.83 GB group host + 85 MB helper + 17 MB start.exe + engine/WebKit) at ~88% CPU. Remaining manual check: play Track 8 on the physical Keystation and confirm sound/meter movement; the process/state/recovery evidence above is from the running app.
- fix: T313 replaces the per-slot host with a page-scoped `PageHost`/`PageHostSlot` and `SharedPage` transport. The helper loads every plugin on the page once, one worker thread per slot, and exposes per-slot audio/control/heartbeats. A Wine page gets a temporary mirrored bundle view with a `vstbridge.toml` group, so all instances share one `vstbridge-host`. The watchdog kills the whole process tree (descendants and processes whose command line names a helper pid) and runs bounded recovery that captures healthy states, restarts the host, reapplies committed states, re-arms the previously active slot, and returns to `ready`. The old per-slot path stays behind `IHUB_SHARED_PLUGIN_HOST=0` / `SessionConfig::sharedPluginHost`. `GET /state` now carries `pages[]`, and `POST /pages/{page}/restart` is the manual recovery entry point. Startup now warms the Wine prefix with `wineserver -p` before launching the helper, `stopProcess(true)` kills every process carrying the prefix in `WINEPREFIX`, and `shutdown()` uses the same reset so a stopped host leaves no Wine processes behind.
- verified: 2026-11-07 debug build, `cmake --build build-debug --target instrument-hub` -> `Linking CXX executable engine/instrument-hub`. After killing the Wine prefix (`wineserver -k`, stale `winedevice`/`start.exe`), `scripts/dev.sh --watch --project /tmp/opencode/track-check.ihub` opened with `pages[0].status: "ready"` and slots 0-7 `warm` in about 30 s. `kill -9` of the `ihub-sandbox` page host reported `recovering`, returned `ready` in 31 s with all eight slots warm and active slot 0, and the old helper, `start.exe`, `wineserver`, `winedevice` and old vstbridge group PIDs were gone; only one new group remained. `POST /pages/tracks/restart` returned `ready` in 33 s and reaped the previous host tree. No orphan Wine processes or defunct wineservers remained.

### BUG-025 · Changing a source away from Analog Lab and back leaves the shared page host failed
- status: fixed
- area: plugins/page, session
- steps:
  1. open the eight-track project with Analog Lab on every track (`/tmp/opencode/track-check.ihub`)
  2. change Track 8's source away from Analog Lab and then back to Analog Lab
  3. wait for the shared page host to restart
- expected: the page host restarts, all eight Analog Lab slots return to `warm`/`active`, and the track keeps working
- actual: `GET /state` reports `pages[0].status: "failed"` with `the shared plugin host exited during startup: ... terminate called without an active exception`; every slot is cold/idle. `POST /pages/tracks/restart` also fails after 60 s. A full app restart that also kills the stale `wineserver` and `vstbridge-host` group recovers, and the project (including track names and the seven captured sounds) loads normally.
- evidence: 2026-11-06 19:53, debug build; page error in `/state`; process list showed a long-lived stale `wineserver` plus the old `start.exe`/`vstbridge-host.exe.so group` after the page host restart. Killing all of them, then launching `scripts/dev.sh --watch`, returned `pages[0].status: "ready"` with slots 0-7 warm. Track 8 had no `plugin-state/slot-7.bin` because the source change cleared it; slots 0-6 kept their captured sounds. Track names Drums/Bass/Keys/Piano/Rhodes/Lead/Strings were preserved.
- fix: `PageHost::stopProcess()` now kills every process whose command line names the page's shared-memory file or its vstbridge group, including that process's descendants, before waiting for the helper. It also removes the temporary bundle view, so the next start writes a fresh `vstbridge.toml` with a new group name instead of reusing a dead group socket. Page startup and recovery run `wineserver -k` for the plugin's Wine prefix, so a crashed `vstbridge-host.exe` group cannot wedge the next host. The watchdog now keeps running after a recovery instead of returning after the first crash, which previously left `/state` reporting `ready` with no host process and therefore no sound. `PageHost::runControlCommand()` marks the slot busy while `applyState`/`captureState`/parameter/editor commands are in flight, and the watchdog skips a busy slot so a slow Analog Lab state restore cannot trigger a false recovery. 2026-11-06 22:57 debug build: after `kill -9` of the page host, the watchdog reported `recovering`, the first restart attempt timed out, the second attempt returned `ready`, the old group was gone, and a VirMIDI note produced a 0.82 peak in the captured Rubix22 monitor.


### BUG-028 · Switching tracks while a note is playing crashes the Wine page host
- status: fixed
- area: plugins/page, session/arm
- steps:
  1. open the eight-track project and arm an instrument with a saved Analog Lab sound
  2. hold a note on the Keystation
  3. switch to another track while the note is still held
- expected: the crossfade moves to the new track and the old note is released
- actual: the note can hang and mute controls stop responding, then `pages[0].status` changes to `recovering`/`warming`; the Wine `vstbridge-host.exe` process has crashed.
- evidence: 2026-11-06 23:17, debug build. `/tmp/ihub-page-host-crash.log` ended with `ihub-sandbox: slot 3 threw in the plugin: read: End of file` and `terminate called ... write: Broken pipe`, preceded by repeated `JUCE Assertion failure in juce_VST3PluginFormatImpl.h:3498` (`restartComponent` off the message thread) and `:2809` (`getStateInformation` off the message thread). `GET /state` showed all eight slots warming while the host restarted.
- fix: page-host control commands (`applyState`, `captureState`, `applyParameters`, editor open/close) now run through `juce::MessageManager::callSync` in both the shared page client and the legacy per-slot client, so VST3 state calls happen on the message thread. The duplicate previous-state capture on arm was removed earlier. Arm-time state capture is gone entirely: switching only moves the active slot; the loaded plugin keeps its live state, and `POST /project`/autosave still capture every slot before writing the file. Arming or disarming now queues all-notes-off, all-sound-off and sustain-off for every page slot, so a lost note-off cannot survive a switch. Page status changes are written to the dev log and an unexpected host exit appends its last output to `/tmp/ihub-page-host-crash.log`. 2026-11-06 23:24 debug build: 20 rapid switches while notes were held kept `pages[0].status` at `ready` with no crash log; a full project save while a note was held also stayed ready. Holding a note, switching away and switching back left slot 5 silent with no stuck voice. The page worker only marshals `handleControl` when the control sequence changes instead of calling `callSync` on every loop, and the watchdog starts a fresh hang window when a slot first has an outstanding request after idling, so the first switch after warm-up cannot be mistaken for a hang.
- verified: 2026-11-07 debug build, `/tmp/opencode/track-check.ihub`, VirMIDI 0-1 enabled through `POST /midi/inputs`. 20 track switches with a held C4 (note-on re-sent before each switch) kept `pages[0].status` at `ready`; after every switch the previous slot's meter peak was `0.0` and its gain `0.0`, all eight slots stayed `warm`, and `/tmp/ihub-page-host-crash.log` did not exist. After the sweep all eight slot meters decayed to `0.0`; a fresh C4 on slot 0 produced a 0.20 peak, and `kill -9` recovery is recorded in BUG-024.

### BUG-029 · Shared page host first recovery attempt stalls at slot 3
- status: fixed
- area: plugins/page, plugins/sandbox, session
- steps:
  1. open the eight-track project and wait for `pages[0].status` to become `ready`
  2. `POST /pages/tracks/restart`
- expected: the page stops, starts one replacement host, restores all eight states and returns to `ready` in one attempt
- actual: the first replacement host stalls with slots 0-2 ready and slot 3 still loading; the request returns `{"error": "the shared plugin page did not recover in time"}` after 180 s and `/state` stays `recovering`; a later attempt reaches `ready` about 290 s after the request
- evidence: 2026-11-07 debug build, eight Analog Lab V tracks. `/tmp/opencode/t314-restart-final3.json` holds the timeout error; the stalled helper page dump showed slot 3 `state=1` (loading) for more than two minutes; the next helper started after the first attempt was killed. The same request returned `ready` in 33 s when the first attempt did not stall (`/tmp/opencode/t314-restart-final4.json`), and `kill -9` recovery returned `ready` in 31 s. Reproduced again on 2026-11-07 21:13: a four-restart run timed out at 155.7 s on the fourth request; the first replacement host page (`ihub-page-e8cef4a9...`) stayed `loading` with a dead `vstbridge-host` while the second attempt reached `ready` in 17 s (`/tmp/opencode/agent-t315-serve.log`). A verbose vstbridge run showed the shared group host overflowing its 1 MB Wine stack during the seventh `IPluginFactory::createInstance` (`20:54:56 ... err:virtual:virtual_setup_exception stack overflow ... 0120:...`, `/tmp/opencode/vstbridge-t315-clean.log`).
- fix: `PageSlot` loading no longer calls `PluginInstrument::loadFromFile()` for every slot. That path did a `findAllTypesForFile()` type scan per slot, and each scan instantiated and destroyed a throwaway Analog Lab V instance in the shared vstbridge group host before the real instance was created. On the first replacement page host the extra create/destroy churn pushed the group host into the Wine stack-overflow/hang above, leaving one slot `loading` until the 120 s startup timeout; the second attempt started from the already-settled group and succeeded. `SandboxPageClient` now caches one `juce::PluginDescription` per bundle path and creates exactly one instance per slot with `PluginInstrument::load()`. The page host also waits for the previous helper's graceful exit, kills the old vstbridge group and Wine-prefix processes, and warms the prefix before launching the replacement (`PageHostProcess`, `PageHostProcessTree`, `PageHostWine`). Verified 2026-11-07 21:20-21:24 on the eight-track project with `--serve`: five `POST /pages/tracks/restart` calls returned `ready` in 21.6-22.2 s, five `kill -9` page-host recoveries returned `ready` in 16.0-17.0 s, and no helper, `start.exe`, `wineserver`, `winedevice` or vstbridge group from the previous host survived any recovery (`/tmp/opencode/agent-t315-accept2.txt`). Five cold manual helper starts loaded all eight slots in 13.6-14.1 s (`/tmp/opencode/agent-t315-helper-D*.log`). The live GUI app on the same project then returned `ready` 23.0 s after a manual restart and 21.5 s after `kill -9` of the page host; the killed helper was gone.

### BUG-030 · Page-host crash log floods with VST3 restartComponent assertions
- status: fixed
- area: plugins/page
- steps:
  1. run eight Analog Lab tracks until `pages[0].status` is `ready`
  2. `kill -9` the `ihub-sandbox` page host
  3. inspect `/tmp/ihub-page-host-crash.log`
- expected: the crash log keeps the host exit reason and a short output tail
- actual: the 12,235-byte tail is 203 copies of `JUCE Assertion failure in juce_VST3PluginFormatImpl.h:3498` (`restartComponent` off the message thread), hiding the useful context
- evidence: 2026-11-07 00:18 kill test; `wc -l /tmp/ihub-page-host-crash.log` -> 205 lines, `grep -c juce_VST3PluginFormatImpl.h:3498` -> 203; the earlier `:2809` `getStateInformation` assertion no longer appears. 2026-11-07 21:44 final debug build: a manual `POST /pages/tracks/restart` returned `ready` in 26.0 s and committed all eight states, then `kill -9` of the replacement helper recovered to `ready` in 20.4 s; `/tmp/ihub-page-host-crash.log` is 84 lines with one `reason: shared plugin host exited unexpectedly` line, exactly 10 `juce_VST3PluginFormatImpl.h:3498` lines, one `[ihub: further repeated restartComponent assertion lines suppressed]` line and the Wine startup output that preceded them, so the useful context is preserved. 20 held-note track switches through VirMIDI 0-1 at 21:44:07-21:44:14 left the page `ready` and created no crash log (`/tmp/opencode/t316-switches.txt`). A controlled probe on the final helper (`/tmp/opencode/t316-gdb-hits.txt`, `/tmp/opencode/t316-probe4-helper.log`) showed the assertion source: two `setStateInformation` applies produced 36 raw `:3498` lines, and the gdb backtrace for `restartComponent(flags=32, kMidiCCAssignmentChanged)` is thread 10 inside `libvstbridge-vst3.so` (`Vst3PluginBridge` message loop), not the JUCE message thread. The first-party state call itself ran through `MessageManager::callSync` on the helper message thread; Analog Lab defers its `restartComponent` to the bridge thread, which a host cannot marshal. A headless probe with `DISPLAY` unset captured state, applied it twice and rendered 300 blocks without any `:2809` `getStateInformation` assertion (`/tmp/opencode/t316-probe5-helper.log`), confirming the first-party state path now runs on the helper message thread even without a display.
- fix: `PageHostProcess` now parses the helper output by line while it is captured, keeps the first ten `juce_VST3PluginFormatImpl.h:3498` lines for each host lifetime, replaces every further copy with one suppression line, and preserves the surrounding Wine output instead of letting the assertions fill the 12 KB tail. `PageHostRecovery` writes `reason: shared plugin host exited unexpectedly` into the crash log even when the output tail is empty and passes only the bounded tail to the page status. `SandboxPageClient` and `SandboxClient` always create the JUCE message manager and run its dispatch loop, even when no display is available, and marshal editor-close state capture and every control command to that thread; this closes the remaining first-party state path that could call VST3 off the message thread. The real host-exit reason and the first ten assertions are still visible, so a genuine crash is not hidden.

### BUG-031 · Persistent wineserver and winedevice survive engine stop
- status: fixed
- area: plugins/page, engine shutdown
- steps:
  1. open the eight-track project and wait for `pages[0].status` to become `ready`
  2. stop the app (`kill -TERM` the `scripts/dev.sh` wrapper, or close the window while the page host is busy)
- expected: the page host, vstbridge group, `start.exe`, `wineserver` and `winedevice` all exit
- actual: after the engine process is gone, `wineserver -p` and two `winedevice.exe` processes remain; a short shutdown window can kill the engine before `PageHost::shutdown()` finishes
- evidence: 2026-11-07 after stopping the dev build, `pgrep -ax instrument-hub` returned nothing while `1244004 wineserver -p`, `1244036 C:\windows\system32\winedevice.exe` and `1244078 C:\windows\system32\winedevice.exe` remained; BUG-025 shows SIGTERM can also leave `start.exe` behind. 2026-11-07 22:02 final debug build, eight-track project `ready`: sending `WM_DELETE_WINDOW` to the app window removed the engine, helper, `start.exe`, vstbridge group, `wineserver -p` and both `winedevice.exe` in 968 ms; `kill -TERM` of a fresh engine removed the same set in 781 ms (an earlier build measured 756 ms). `kill -9` of helper 316623 recovered to `ready` in 18 s, with old helper 316623, `start.exe` 316625, vstbridge group 316657 and `wineserver -p` 316299 all gone and exactly one new group running.
- fix: `PageHost::stopProcess` always runs `stopWinePrefixes` (`wineserver -k`) and then `killProcessesWithWinePrefix` before it waits for or kills the vstbridge group, so the persistent prefix server and its `winedevice.exe` children are killed before the engine exits. The group wait and the process matching timeout were shortened so shutdown does not depend on six seconds of graceful waiting, `PageHost::shutdown` uses a 2 s helper grace period, and `runControlCommand` stops waiting for an in-flight command when shutdown begins.

### BUG-032 · MIDI input changes flood the log with device-list assertions
- status: fixed
- area: midi
- steps:
  1. launch the app through Vite and wait for the UI
  2. `POST /midi/inputs {"id":"17-0","enabled":true}`, or refresh the MIDI device list
- expected: the input opens without JUCE assertions
- actual: `JUCE Assertion failure in juce_MidiDeviceListConnectionBroadcaster.cpp:94` and `:227` repeat continuously in the dev log
- evidence: 2026-11-07 `/tmp/opencode/t314-dev10.log` had 576 occurrences of each assertion; VirMIDI 0-1 still opened and delivered notes. Before the fix the live dev log `/tmp/opencode/t315-final-dev.log` had grown to 2,397 occurrences of each `:94`/`:227` line; an explicit disable/enable of 17-0 also produced `:82`, `:245` and `juce_UMPInput.cpp:56/111/165/174` assertions. After the fix (debug build 22:03): `POST /midi/inputs {"id":"17-0","enabled":false}` then `{"id":"17-0","enabled":true}` both returned 200, a 10 s wait added no log lines, and a `GET /midi` plus two note-on/off pairs through `amidi -p hw:0,1` reported `17-0` enabled/open with 4 events and `lastEvent: note off 64`; the window and the whole log since the app restart contain zero `MidiDeviceListConnectionBroadcaster` and zero `JUCE Assertion failure` lines (`/tmp/opencode/t318-accept.txt`).
- fix: `/midi` requests call `MidiBridge::snapshot()` on an httplib worker thread, and that path (plus `POST /midi/inputs`) called `juce::MidiInput::getAvailableDevices()`, `juce::MidiInput::openDevice()` and the `juce::MidiInput` destructor off the JUCE message thread; the broadcaster, UMP input and their consumers are all message-thread affine. `JuceMidiInputBackend` now runs device enumeration, device opening and input teardown through `juce::MessageManager::callSync`, which executes directly when already on the message thread and marshals otherwise. `start()`/`stop()` remain the lock-free flag flips inside JUCE, the MIDI callback and audio path are unchanged, and the HTTP thread only blocks until the short JUCE call returns.
