# Next-session tasks (manual, not autopilot queue)

Human-requested backlog from 2026-11-04. These are for interactive sessions, not the
autopilot driver. Do not add them to `plan/STATE.json`. One task per session.

Current state for context:

- Browser UI runs against `build-debug/engine/instrument-hub --serve`.
- Engine currently on **Rubix22 Pro** via JACK, `pro-audio` PipeWire profile, 96 frames,
  5.0 ms play budget, 7.29 ms measured device round trip, 0 xruns.
- The UR22 measured 10.375 ms at the same 96-frame setting.
- Launchpad/Keystation show “detected, not connected” in the browser because
  `Main.cpp` only starts `ControllerSurface` for `AppMode::gui`, never for `serve`.
- The measure control now has a source choice: selected device (physical out→in cable)
  or internal ALSA loopback.
- The Instrument source picker now offers live audio input (Input 1/2). It is wired but
  has no docs pass yet.

## T308 · Attach controllers in serve/browser mode

- depends: none
- size: M
- goal: Make the Launchpad Mini MK3 and Keystation work when the engine runs in
  `--serve` mode for the browser/remote UI. Start `ControllerSurface` for
  `AppMode::serve` as well as `AppMode::gui`, run its LED refresh timer in serve mode,
  and keep GUI mode unchanged. Investigate whether `ControllerSurface::start` needs any
  window or message-loop assumptions and remove them if so.
- files: `engine/src/Main.cpp`, `engine/src/app/ControllerSurface.*`, `ui/src/features/midi/`
- accept: with `--serve`, `GET /midi` reports `launchpad.connected=true`,
  `launchpad.handshake=true`, `inputName`/`outputName` set; enabling the MI row works;
  pressing pads arms/switches tracks and LEDs repaint; Keystation notes reach the armed
  slot; the browser MIDI page no longer says “driver is not attached”; GUI mode still
  connects. Ask before builds.

## T309 · Audio auto-setup wizard

- depends: none
- size: L
- goal: On Audio & MIDI, add an **Analyze & recommend** action that probes the connected
  soundcard at 64/96/128 frames (and available rates), measures round trip and xruns,
  and recommends the lowest stable setting. Show a dialog with the recommendation and
  one **Apply** button that sets device type/name, sample rate, buffer and channels.
  Store the accepted profile per soundcard so each interface reopens with its own best
  settings. `pro-audio`/JACK profile selection for class-compliant interfaces should be
  part of the profile or documented as a prerequisite. Audio settings are global; only
  FX-chain latency is per track — ask the developer whether “per instrument” means per
  soundcard or per track before implementing.
- files: `engine/src/api/state/AudioState.h`, `engine/src/api/routes/AudioRoutes.cpp`,
  `engine/src/audio/settings/`, `engine/src/app/ControlApi.cpp`, `ui/src/features/audio-settings/`,
  `shared/api/openapi.yaml`
- accept: with the Rubix22, the wizard recommends 96 frames/pro-audio and applying it
  reproduces the 7.29 ms / 0 xruns state; with a bad buffer it reports xruns and picks
  the stable one; the dialog explains the tradeoff; profiles survive restart; legacy
  ALSA loopback still works; ask before builds.

## T310 · Generic MIDI controllers and compact settings

- depends: T308
- size: L
- goal: Treat controllers as generic MIDI where possible. Any keyboard input can play
  the armed slot without a device-specific Keystation role. Any 64-pad grid controller
  can drive the grid through a learn mode (press pads in order) or a configurable note
  base; LED output stays limited to protocols we implement (Launchpad programmer mode).
  Replace the large Launchpad/Keystation status cards with a compact input list showing
  role, enabled state and handshake status.
- files: `ui/src/features/midi/`, `engine/src/api/MidiBridge.*`,
  `engine/src/controller/launchpad/`, `engine/src/controller/mapping/`
- accept: a generic USB keyboard plays notes; a generic 64-note controller can be learned
  and triggers the same arm/switch paths; Launchpad LEDs still work; the MIDI page is a
  compact list; unusable/unknown controllers fail with a readable reason; ask before
  builds.

## T311 · Finish live audio input track

- depends: none
- size: M
- goal: Finish and document the live audio-input source under the track layout. Verify
  the new “Live audio input · Input 1/2” source assigns an `AudioInputInstrument`, the
  track’s FX chain processes it, track volume/arm works, and the source persists in the
  project. Add a short doc for the guitar/looper workflow.
- files: `ui/src/features/instrument/`, `engine/src/session/SessionBuild.cpp`,
  `engine/src/session/SessionControl.cpp`, `docs/`
- accept: assigning live input to an empty track and playing through its inserts works;
  it survives save/restart; the selected-track rule is documented; ask before builds.

## T312 · Persist the low-latency PipeWire profile

- depends: none
- size: S
- goal: Make the Rubix22 `pro-audio` profile and the `Rubix22 Pro` JACK device the
  repeatable startup path. Add a repo script or startup helper that
  `pactl set-card-profile <card> pro-audio`, then launches the engine with
  `--audio-type JACK --audio-device "Rubix22 Pro" --audio-buffer-size 96` through
  `pw-jack`. Document the UR22/Rubix22 fallback and the Linux class-driver limits.
- files: `scripts/`, `docs/`, `plan/DECISIONS.md` if a decision changes
- accept: a fresh login/PipeWire restart can start the app at the Rubix22 96-frame
  setting with one command; no `~/.config` edits without the developer’s explicit
  approval; ask before builds.

## T313 · Shared plugin host per instrument page

- depends: BUG-024, T307
- size: XL
- goal: Host every plugin instrument on a track page in one shared
  `ihub-sandbox`/Wine process instead of one sandbox per slot. All instruments
  on the page stay warm; switching is still only the 2–5 ms gain ramp. Add a
  page-level watchdog and automatic recovery: if the shared host dies or hangs,
  mute the page, restart the host, restore every slot’s committed plugin state,
  re-arm the active track and fade back in without crashing or blocking the
  audio thread. Design the host for N slots/page so future instrument pages can
  switch as a group; do not hard-code eight.
- files: `engine/src/plugins/SandboxClient.*`,
  `engine/src/plugins/sandbox/`, `engine/src/plugins/sandbox_main.cpp`,
  `engine/src/session/SessionBuild.cpp`, `engine/src/session/WarmPoolController.*`,
  `engine/src/audio/warm/`, `engine/src/api/state/`,
  `ui/src/features/grid/`,
  `docs/architecture.md`
- accept: eight Analog Lab V tracks warm in one host process; switching all
  eight produces no loads/warming and no xruns; `kill -9` the host recovers all
  eight states and re-arms the active track without crashing or leaving orphan
  Wine processes; RSS is materially lower than eight separate hosts; verify
  in the app.
- full brief: `plan/T313-SHARED-PLUGIN-HOST.md`
