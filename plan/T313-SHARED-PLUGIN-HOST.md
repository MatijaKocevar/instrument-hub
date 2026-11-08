# T313 · Shared plugin host per instrument page

- depends: BUG-024, T307
- size: XL
- status: implemented (human-requested 2026-11-05; audio verification pending)

## Why

The track layout currently gives every plugin slot its own `ihub-sandbox` +
Wine/vstbridge host. Eight warmed Analog Lab V tracks therefore mean eight
independent Wine processes, all loading the same Arturia preset database. In
practice the eighth host can deadlock in `applyState` and is killed by the
watchdog, so Track 8 is silent (BUG-024). The same design also multiplies RAM
and CPU: eight Wine hosts at roughly 1.2 GB RSS each.

Ableton-style hosts load multiple VST3 instances into one plugin process. That
is the model we want:

- one plugin host process per instrument page,
- every instrument on the page warmed and ready,
- switching only changes which slot is audible (2–5 ms gain ramp),
- if the shared host dies, every slot on the page goes silent, the engine
  restarts the host automatically, restores all slot states and re-arms the
  current track. The developer has accepted that failure mode.

This also lays the groundwork for **pages of instruments**: future controller
pages can each own a set of instruments and switch as a group instead of
assuming exactly eight slots.

## Goal

Replace the per-plugin-slot sandbox with a page-scoped shared plugin host:

1. One `ihub-sandbox` process hosts every plugin instrument on the current
   page (one page = the eight track slots for now).
2. The engine still exposes one `SlotSource` per slot; the live audio graph,
   gain ramp and per-slot state are unchanged.
3. All instruments on the page are warmed. Switching between them performs no
   load, no plugin instantiation and no state apply.
4. A watchdog detects a dead or hung shared host, mutes the page, restarts the
   host, reloads each slot's last committed plugin state, re-arms the active
   track and fades back in. Recovery must not crash or block the engine.
5. The host API is page-shaped, not hard-coded to eight slots, so later pages
   can be added without redesigning the process model.

## Non-goals

- This is not a multitrack mixer. Only one armed slot is audible at a time.
- Do not change the fixed audio graph or the 2–5 ms gain ramp.
- Do not re-enable Wine plugin editors (BUG-019 still applies).
- Do not add a package-manager dependency.
- Do not implement the pages UI in this task; only make the host capable of N
  slots and a page descriptor.

## Current path to start from

- `Session::createPooledSource` creates one `plugins::SandboxHost` per plugin
  slot (`engine/src/session/SessionBuild.cpp`).
- `SandboxHost::start` launches `ihub-sandbox --map <shm> --plugin <bundle>`
  (`engine/src/plugins/sandbox/SandboxHostProcess.cpp`).
- `SandboxClient::run` loads one plugin and runs one request/reply ring
  (`engine/src/plugins/SandboxClient.*`, `engine/src/plugins/sandbox_main.cpp`).
- The watchdog, status callbacks and failure handling are in
  `engine/src/plugins/sandbox/SandboxWatchdog.cpp`,
  `SandboxHostStatus.cpp` and `engine/src/session/SessionPluginReload.cpp`.
- The warm pool and switcher are under `engine/src/audio/warm/` and
  `engine/src/session/WarmPoolController.*`.

## Design requirements

### Process model

- Add a shared sandbox host that owns one child process for a page. The child
  loads all plugin instances for that page in the same process.
- Keep a per-slot shared-memory audio/control channel so the engine audio
  thread still reads/writes per slot with no locks or allocation. Either extend
  the helper to map one region per slot or use one region with per-slot rings;
  document the chosen layout.
- Do not spawn one Wine/vstbridge host per slot. With eight Analog Lab V
  instances there must be exactly one `vstbridge-host` process for the page.
- Keep the old per-slot path behind a migration flag
  (`sharedPluginHost`, default on for the track layout) until the new path has
  been proven in the app; remove the flag when the old path is no longer needed.

### Warm pool and switching

- Every plugin slot on the page is a warm source backed by the shared host.
- Arming a slot must not create, destroy or reconfigure a plugin instance; it
  only changes the switcher target and gain ramp.
- The shared host must expose per-slot state/metadata so `GET /state` can still
  report `warm`, `state`, latency and per-slot errors.

### Recovery

- A page-level watchdog watches the shared child process and its heartbeats.
- On failure or hang:
  1. immediately mute every slot on the page and report `recovering`;
  2. capture any healthy slot states that can still be read;
  3. stop and reap the old child and all Wine descendants (no orphan
     `vstbridge-host` processes);
  4. restart the shared host and reload every slot's last committed state;
  5. re-arm the slot that was active before the failure;
  6. fade back in and clear the error.
- Recovery runs off the audio thread. The audio callback must keep producing
  silence for the page without locking, allocating or logging.
- Bound the retries (for example three attempts with backoff). If recovery
  fails, keep the engine alive, report a readable page error and leave the
  slots failed. Never loop forever and never abort the app.
- A manual `POST /pages/{page}/restart` (or the existing
  `POST /patches/{slot}/reload`) must trigger the same recovery path.

### API and UI

- Extend `GET /state` (and the `state` WebSocket event) with a page/plugin-host
  status: `ready`, `recovering`, `failed`, plus the affected slot indices.
- During recovery, the track grid should show a clear “Recovering instruments…”
  banner and keep the last selected track highlighted. Do not show eight
  separate failures for one shared host.
- Once recovery succeeds, all slots return to `warm` with their original
  states and the banner disappears.

### Pages

- The shared host takes a page descriptor: page id, slot indices, plugin
  descriptors and states. Do not hard-code the number eight in the host or the
  recovery path.
- Design the page lifecycle so a future page switch can either reuse one host
  or stop the old page and start the new one. Document the chosen policy.
- A page must be able to hold fewer than eight instruments (empty slots stay
  silent) and, later, any page size the controller mapping uses.

## Acceptance

1. **One host:** with eight Analog Lab V tracks warm, `ps` shows one
   `vstbridge-host` process for the page, not eight; all eight slots report
   `warm: true` and no errors.
2. **Instant switching:** arming all eight tracks in sequence produces no
   `cold`/`warming` transitions, no plugin loads and no xruns; switching is
   still the existing gain ramp.
3. **Crash recovery:** `kill -9` the shared host. The page reports
   `recovering`, audio mutes, the engine restarts the host, restores all eight
   slot states, re-arms the previously active track and returns to `ready`
   without crashing. No orphan `vstbridge-host` processes remain.
4. **Hang recovery:** simulate a hung instance or `SIGSTOP` the host. The
   watchdog recovers the page the same way and reports the failure if the
   retry budget is exhausted.
5. **State:** per-slot capture, save, project reopen and restore still work;
   each track keeps its own Analog Lab sound.
6. **Efficiency:** record RSS and CPU for the eight-instance page before and
   after. The shared host must use materially less memory than eight separate
   Wine hosts.
7. **Compatibility:** the legacy 64-patch family grid and existing sandbox
   diagnostics keep working. Verify in the app.
8. **Docs:** update `docs/architecture.md`, `plan/BUGS.md` (BUG-024 status and
   evidence) and any decision record if the isolation model changes.

## Suggested sequence

1. Extend the sandbox protocol/helper to load multiple plugin instances in one
   process and expose per-slot audio/control channels.
2. Add the engine-side shared host + per-slot proxy sources, behind the
   migration flag; keep the old path green.
3. Wire the track layout to one shared host per page; verify eight plugin
   instances, then eight Analog Lab V instances.
4. Add page status to the API/events and the recovering banner.
5. Add watchdog + restart + state restore + re-arm + bounded retry.
6. Verify crash, hang, state restore, switching and memory in the app; update docs.

## Risks to resolve during the task

- The helper currently assumes one plugin bundle per process; confirm the
  sandbox can host several VST3 instances without vstbridge starting one Wine
  host per instance.
- Shared failure takes the whole page down; keep the old per-slot path behind
  the flag until recovery is solid.
- Wine process-tree cleanup is currently incomplete: killing `ihub-sandbox`
  leaves the `vstbridge-host` descendant alive. The shared host must reap the
  whole tree.
- The audio-thread contract is non-negotiable: all failure detection and
  recovery stays on worker/message threads.

## Result

- The page transport is one `SharedPage` with an array of per-slot channels
  (`SharedMemory slots[]`), one helper process, one worker thread per slot and
  one vstbridge group per page. `PageHost` supports 32 slots and takes a
  vector of descriptors, so nothing assumes eight.
- The track layout uses it by default. `IHUB_SHARED_PLUGIN_HOST=0` or
  `SessionConfig::sharedPluginHost=false` keeps the legacy per-slot path.
- `GET /state` now carries `pages[]` (`ready`, `recovering`, `failed`, affected
  slots and page error). The track grid shows a “Recovering instruments…”
  banner and keeps the selected track highlighted.
- Measured with the eight Analog Lab V track project on this machine:
  eight legacy hosts = 9,962 MB and ~211% CPU; one shared page = 2,187 MB and
  ~88% CPU. Killing the group host recovered all eight states and the active
  track in 17 s (two retries while the old group socket cleared).
 T313 · Shared plugin host per instrument page

- depends: BUG-024, T307
- size: XL
- status: implemented (human-requested 2026-11-05; audio verification pending)

## Why

The track layout currently gives every plugin slot its own `ihub-sandbox` +
Wine/vstbridge host. Eight warmed Analog Lab V tracks therefore mean eight
independent Wine processes, all loading the same Arturia preset database. In
practice the eighth host can deadlock in `applyState` and is killed by the
watchdog, so Track 8 is silent (BUG-024). The same design also multiplies RAM
and CPU: eight Wine hosts at roughly 1.2 GB RSS each.

Ableton-style hosts load multiple VST3 instances into one plugin process. That
is the model we want:

- one plugin host process per instrument page,
- every instrument on the page warmed and ready,
- switching only changes which slot is audible (2–5 ms gain ramp),
- if the shared host dies, every slot on the page goes silent, the engine
  restarts the host automatically, restores all slot states and re-arms the
  current track. The developer has accepted that failure mode.

This also lays the groundwork for **pages of instruments**: future controller
pages can each own a set of instruments and switch as a group instead of
assuming exactly eight slots.

## Goal

Replace the per-plugin-slot sandbox with a page-scoped shared plugin host:

1. One `ihub-sandbox` process hosts every plugin instrument on the current
   page (one page = the eight track slots for now).
2. The engine still exposes one `SlotSource` per slot; the live audio graph,
   gain ramp and per-slot state are unchanged.
3. All instruments on the page are warmed. Switching between them performs no
   load, no plugin instantiation and no state apply.
4. A watchdog detects a dead or hung shared host, mutes the page, restarts the
   host, reloads each slot's last committed plugin state, re-arms the active
   track and fades back in. Recovery must not crash or block the engine.
5. The host API is page-shaped, not hard-coded to eight slots, so later pages
   can be added without redesigning the process model.

## Non-goals

- This is not a multitrack mixer. Only one armed slot is audible at a time.
- Do not change the fixed audio graph or the 2–5 ms gain ramp.
- Do not re-enable Wine plugin editors (BUG-019 still applies).
- Do not add a package-manager dependency.
- Do not implement the pages UI in this task; only make the host capable of N
  slots and a page descriptor.

## Current path to start from

- `Session::createPooledSource` creates one `plugins::SandboxHost` per plugin
  slot (`engine/src/session/SessionBuild.cpp`).
- `SandboxHost::start` launches `ihub-sandbox --map <shm> --plugin <bundle>`
  (`engine/src/plugins/sandbox/SandboxHostProcess.cpp`).
- `SandboxClient::run` loads one plugin and runs one request/reply ring
  (`engine/src/plugins/SandboxClient.*`, `engine/src/plugins/sandbox_main.cpp`).
- The watchdog, status callbacks and failure handling are in
  `engine/src/plugins/sandbox/SandboxWatchdog.cpp`,
  `SandboxHostStatus.cpp` and `engine/src/session/SessionPluginReload.cpp`.
- The warm pool and switcher are under `engine/src/audio/warm/` and
  `engine/src/session/WarmPoolController.*`.

## Design requirements

### Process model

- Add a shared sandbox host that owns one child process for a page. The child
  loads all plugin instances for that page in the same process.
- Keep a per-slot shared-memory audio/control channel so the engine audio
  thread still reads/writes per slot with no locks or allocation. Either extend
  the helper to map one region per slot or use one region with per-slot rings;
  document the chosen layout.
- Do not spawn one Wine/vstbridge host per slot. With eight Analog Lab V
  instances there must be exactly one `vstbridge-host` process for the page.
- Keep the old per-slot path behind a migration flag
  (`sharedPluginHost`, default on for the track layout) until the new path has
  been proven in the app; remove the flag when the old path is no longer needed.

### Warm pool and switching

- Every plugin slot on the page is a warm source backed by the shared host.
- Arming a slot must not create, destroy or reconfigure a plugin instance; it
  only changes the switcher target and gain ramp.
- The shared host must expose per-slot state/metadata so `GET /state` can still
  report `warm`, `state`, latency and per-slot errors.

### Recovery

- A page-level watchdog watches the shared child process and its heartbeats.
- On failure or hang:
  1. immediately mute every slot on the page and report `recovering`;
  2. capture any healthy slot states that can still be read;
  3. stop and reap the old child and all Wine descendants (no orphan
     `vstbridge-host` processes);
  4. restart the shared host and reload every slot's last committed state;
  5. re-arm the slot that was active before the failure;
  6. fade back in and clear the error.
- Recovery runs off the audio thread. The audio callback must keep producing
  silence for the page without locking, allocating or logging.
- Bound the retries (for example three attempts with backoff). If recovery
  fails, keep the engine alive, report a readable page error and leave the
  slots failed. Never loop forever and never abort the app.
- A manual `POST /pages/{page}/restart` (or the existing
  `POST /patches/{slot}/reload`) must trigger the same recovery path.

### API and UI

- Extend `GET /state` (and the `state` WebSocket event) with a page/plugin-host
  status: `ready`, `recovering`, `failed`, plus the affected slot indices.
- During recovery, the track grid should show a clear “Recovering instruments…”
  banner and keep the last selected track highlighted. Do not show eight
  separate failures for one shared host.
- Once recovery succeeds, all slots return to `warm` with their original
  states and the banner disappears.

### Pages

- The shared host takes a page descriptor: page id, slot indices, plugin
  descriptors and states. Do not hard-code the number eight in the host or the
  recovery path.
- Design the page lifecycle so a future page switch can either reuse one host
  or stop the old page and start the new one. Document the chosen policy.
- A page must be able to hold fewer than eight instruments (empty slots stay
  silent) and, later, any page size the controller mapping uses.

## Acceptance

1. **One host:** with eight Analog Lab V tracks warm, `ps` shows one
   `vstbridge-host` process for the page, not eight; all eight slots report
   `warm: true` and no errors.
2. **Instant switching:** arming all eight tracks in sequence produces no
   `cold`/`warming` transitions, no plugin loads and no xruns; switching is
   still the existing gain ramp.
3. **Crash recovery:** `kill -9` the shared host. The page reports
   `recovering`, audio mutes, the engine restarts the host, restores all eight
   slot states, re-arms the previously active track and returns to `ready`
   without crashing. No orphan `vstbridge-host` processes remain.
4. **Hang recovery:** simulate a hung instance or `SIGSTOP` the host. The
   watchdog recovers the page the same way and reports the failure if the
   retry budget is exhausted.
5. **State:** per-slot capture, save, project reopen and restore still work;
   each track keeps its own Analog Lab sound.
6. **Efficiency:** record RSS and CPU for the eight-instance page before and
   after. The shared host must use materially less memory than eight separate
   Wine hosts.
7. **Compatibility:** the legacy 64-patch family grid and existing sandbox
   diagnostics keep working. Verify in the app.
8. **Docs:** update `docs/architecture.md`, `plan/BUGS.md` (BUG-024 status and
   evidence) and any decision record if the isolation model changes.

## Suggested sequence

1. Extend the sandbox protocol/helper to load multiple plugin instances in one
   process and expose per-slot audio/control channels.
2. Add the engine-side shared host + per-slot proxy sources, behind the
   migration flag; keep the old path green.
3. Wire the track layout to one shared host per page; verify eight plugin
   instances, then eight Analog Lab V instances.
4. Add page status to the API/events and the recovering banner.
5. Add watchdog + restart + state restore + re-arm + bounded retry.
6. Verify crash, hang, state restore, switching and memory in the app; update docs.

## Risks to resolve during the task

- The helper currently assumes one plugin bundle per process; confirm the
  sandbox can host several VST3 instances without vstbridge starting one Wine
  host per instance.
- Shared failure takes the whole page down; keep the old per-slot path behind
  the flag until recovery is solid.
- Wine process-tree cleanup is currently incomplete: killing `ihub-sandbox`
  leaves the `vstbridge-host` descendant alive. The shared host must reap the
  whole tree.
- The audio-thread contract is non-negotiable: all failure detection and
  recovery stays on worker/message threads.

## Result

- The page transport is one `SharedPage` with an array of per-slot channels
  (`SharedMemory slots[]`), one helper process, one worker thread per slot and
  one vstbridge group per page. `PageHost` supports 32 slots and takes a
  vector of descriptors, so nothing assumes eight.
- The track layout uses it by default. `IHUB_SHARED_PLUGIN_HOST=0` or
  `SessionConfig::sharedPluginHost=false` keeps the legacy per-slot path.
- `GET /state` now carries `pages[]` (`ready`, `recovering`, `failed`, affected
  slots and page error). The track grid shows a “Recovering instruments…”
  banner and keeps the selected track highlighted.
- Measured with the eight Analog Lab V track project on this machine:
  eight legacy hosts = 9,962 MB and ~211% CPU; one shared page = 2,187 MB and
  ~88% CPU. Killing the group host recovered all eight states and the active
  track in 17 s (two retries while the old group socket cleared).
- Remaining manual check: play Track 8 from the physical Keystation and
  confirm meter/audio after recovery.
