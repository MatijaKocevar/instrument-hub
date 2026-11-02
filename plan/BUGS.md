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
- status: open
- area: instrument
- steps: repeat the manual checklist `docs/manual-analog-lab-64.md`
- expected: all 64 Analog Lab patches restore their own preset across a full pad sweep
- actual: automated coverage is 8 TestPlugin patches plus 2 real Analog Lab patches; loading 64 Analog Lab instances in one process stack-overflows the Wine bridge
- evidence: `ihub-analog-lab-state-test` (2 patches) passes; `ihub-per-patch-state-test` (8 patches) passes
- fix: pending; candidate is one isolated child process per batch, or manual verification

### BUG-003 · Global page header and connection bar repeat on every page
- status: fixed
- area: ui
- steps: open any desktop route (`/grid`, `/instrument`, `/plugins`, `/audio`, `/settings`)
- expected: the page starts with its own content; app identity and connection state live in the sidebar/footer
- actual: every route prints the "Audio workspace / Instrument Hub / One place…" hero header and a full-width Connected / URL / Retry now bar
- evidence: screenshot in the bug report; `ui-shell-*` and page artifacts under `tests/artifacts/`; T140 harness asserts no `app__header` or `connection-status` on all five desktop routes; `ihub-ui-snapshot` captures the populated 1200×800 Grid
- fix: T140 removes the global hero header and connection bar, keeps the phase in the sidebar and footer, and probes `sidebar-title` / grid cells instead of `app-title`

### BUG-004 · App does not exit after the window is closed; hangs in shutdown
- status: open
- area: engine
- steps:
  1. start `build-release/engine/instrument-hub` in GUI mode and play with Analog Lab patches (log `/tmp/opencode/run-launchpad.log`)
  2. close the window a few minutes later
- expected: the window disappears and the process exits within a second, reaping the vstbridge host
- actual: window stays mapped but never repaints; the process keeps running; `127.0.0.1:8080` is no longer listening, so `shutdown()` already reached `controlApi.stopServer()`; the main thread sits in `futex_wait_queue`; one thread burns ~100% CPU; `WM_DELETE_WINDOW` is ignored; only `SIGTERM` ends it
- evidence: launched 2026-11-01 17:08, found stuck at 17:21 (PID 61425, 14 min elapsed, 8 threads, `/proc/61425/task/61425/wchan` = `futex_wait_queue`, thread 61428 at 99.9%); `ss -ltnp` shows no 8080 listener; `xwininfo` reports the window `IsViewable` at 3840+24; a hand-sent `WM_DELETE_WINDOW` (`/tmp/opencode/xclose 0x560000c`) was ignored; `kill -TERM 61425` exited immediately; `/tmp/opencode/run-launchpad.log` stops at 17:11:00
- fix: pending; `Server::stop()` already got past `controlThread.join()` and `stopListenerLocked()`, so the next candidates are `meterThread.join()`, `controller.shutdown()`, or `session->closeDevice()` joining the audio/Wine plugin thread

### BUG-005 · Engine aborts when the Wine plugin bridge dies
- status: reproduced
- area: engine
- steps:
  1. start the app in development and arm an Analog Lab patch so its `vstbridge-host` is running (engine 450923, host 452420, 21:53)
  2. `kill -9` the `vstbridge-host` process
- expected: the slot reports a failed plugin and the engine keeps running; a hostile plugin that calls `abort()` already dies alone in `ihub-sandbox` (`tests/plugins/SandboxTest.cpp`)
- actual: the engine dies with SIGABRT in the same second; the control API goes away, and because Vite still serves `:5173` the UI loads but every request fails, which reads as "the dev server stopped working"
- evidence: two identical events, both at the second the host was killed — `20:16:12Z WATCHDOG killing stuck vstbridge-host pid=400966` with `/tmp/instrument-hub-crash.log` rewritten at 21:16:12 local, and `20:55:20Z ... pid=452420` with the dump rewritten at 21:55:20 (`signal: 6 SIGABRT`, `records: 0`); only `engine/src/Main.cpp` installs the dump handler, so the abort was the main process, not `ihub-sandbox` and not Wine; after 21:55 no `build-debug` process remained
- fix: pending; `SessionBuild.cpp` builds every plugin slot as an in-process `plugins::PluginInstrument`, and `SandboxHost` (`ihub-sandbox`, T110) is used only by its own test, so a bridge that dies leaves its in-process client without a peer. Next step is a controlled `kill -9` of the host with the engine built with symbols; the empty `records` means the abort never reached the log ring
