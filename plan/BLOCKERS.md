# BLOCKERS — what needs a human

Append-only. The autopilot adds entries; only a human removes them.

Status legend: `open` · `resolved`

---

## B001 · C++ dev headers need sudo
- task: T000
- severity: hard
- status: **resolved 2026-10-31**
- what: `pkg-config`, ALSA, freetype, WebKitGTK and GL development headers were missing.
- why: JUCE cannot compile audio or the embedded web view without them. Installing requires `sudo`.
- human did: installed `build-essential pkg-config libasound2-dev libjack-jackd2-dev libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev libcurl4-openssl-dev ninja-build cmake`, plus `/etc/security/limits.d/99-audio-realtime.conf` and membership of the `audio` group.
- verified: `cmake 4.2.3`, `ninja 1.13.2`, `g++ 15.2.0`, `pkg-config 2.5.1`; alsa/jack/freetype2/x11/gl/webkit2gtk-4.1 all resolve.
- note: `realtime-privileges` does not exist on Ubuntu 26.04. `rtprio`/`memlock` are granted via the limits file instead. Group membership needs a re-login to apply; until then audio tests run without `SCHED_FIFO`.
- workaround: none needed.

## B002 · Launchkey Mini MK3 physical verification
- task: T042
- severity: soft
- status: **superseded 2026-10-31 by B009** (Launchpad Mini MK3 is the controller target; this entry is kept as history)
- what: LED palette velocities and the session-mode SysEx are implemented from the Programmer's Reference and tested against a simulator. Real firmware may disagree.
- why: No hardware attached to the build machine.
- human must: connect the Mini MK3, run the driver, confirm (a) pads light in the expected colours, (b) pads report in session mode, (c) encoders move macros, (d) keys reach the armed instrument. Note any mismatch; the byte tables live in `LaunchkeyProtocol.cpp`.
- workaround: simulator-based tests cover everything except firmware quirks.

## B003 · Analog Lab Pro and EZdrummer 3 validation
- task: T022
- severity: soft
- status: open
- what: Third-party plugins are not installed and cannot be licensed on this machine.
- why: Windows/macOS-only, licence-bound.
- human must: on the Windows music PC, load each in Instrument Hub, confirm editor opens, presets load, state saves and reloads, and note the reported latency.
- workaround: an in-house `TestPlugin` VST3 covers the host contract.

## B004 · Real audio interface tuning
- task: T012
- severity: soft
- status: open
- what: All latency and xrun figures come from the ALSA loopback device.
- why: Your interface is not connected.
- human must: connect the real interface, try 64/128/256 at 48 kHz, record the xrun counts and the measured round-trip, and set the default in `DECISIONS.md` if it differs.
- workaround: loopback numbers are a floor, not a promise.

## B005 · Windows and macOS runtime verification
- task: T004
- severity: soft
- status: open
- what: CI compiles all three platforms but nothing executes on Windows or macOS.
- why: No such machines available to the run.
- human must: run the CI artifacts on each OS; confirm window opens, audio device enumerates, plugin scan works.
- workaround: CI build success only.

## B006 · JUCE licence decision
- task: none
- severity: soft
- status: open
- what: JUCE 8 is AGPLv3 or commercial.
- why: Matters only if you distribute a closed-source binary.
- human must: decide open-source (AGPL, publish the engine source) or buy a JUCE licence. Private use is unaffected.
- workaround: none needed until distribution.

## B007 · Tablet remote physical test
- task: T100
- severity: soft
- status: open
- what: The remote is tested in a desktop browser at tablet viewport sizes.
- why: No tablet on this machine.
- human must: scan the QR from the tablet, confirm pairing, set list, tempo control, and that pad presses on the Launchkey highlight on the tablet.
- workaround: responsive snapshots.

## B008 · Chinese public holidays
- task: none
- severity: soft
- status: open
- what: Off-peak scheduling treats Chinese public holidays as normal weekdays.
- why: Holiday calendar is not computed.
- human must: add dates to `HOLIDAYS` in `autopilot.py` if a run lands on one.
- workaround: a few peak-priced hours per year.

## B009 · Launchpad Mini MK3 physical verification (supersedes B002)
- task: T117
- severity: soft
- status: open
- what: The controller target is the Novation **Launchpad Mini MK3** (64-pad 8x8 grid, no keys, no encoders). B002 describes the superseded Launchkey Mini MK3 and must not be used as the checklist. The Launchpad protocol, driver, 64-pad mapping, control rows and the pad-to-audio path are implemented and tested against scripted MIDI only.
- why: No Launchpad is attached to the build machine. Real firmware may disagree on programmer-mode entry, LED palette values, port naming or CC routing.
- human must: connect the Launchpad Mini MK3 on the Linux host and run `./build/engine/instrument-hub`; confirm (a) the programmer-mode SysEx is accepted and the surface leaves Live mode, (b) grid pads report notes 11..88 on the `MI` port and release as Note On velocity 0, (c) idle/warming/active/error LEDs are the expected off/yellow/green/red, (d) top-row CC 91..98 selects macro banks and right-column CC 89..19 selects scenes, (e) pads 0..7 arm audible slots. Record any byte mismatch; tables live in `engine/src/controller/LaunchpadProtocol.cpp` and the reference in `docs/launchpad-mini-mk3.md`.
- note: B007's tablet check refers to Launchpad pad presses, not Launchkey presses.
- note: the shipped mapping declares 64 pads/slots but the default composed session owns eight slots, so pads 8..63 resolve in the mapping and are out of range for the current graph. This is a code gap, not a hardware blocker; see `HANDOVER.md` section 2.
- workaround: `ihub-launchpad-protocol`, `ihub-launchpad-mapping`, `ihub-launchpad-driver` and `ihub-e2e` cover the protocol, the full mapping, the scripted LED stream and the rendered/captured audio path without hardware.

## B010 · Native page frames and page interaction
- task: T127
- severity: soft
- status: open
- what: The Grid, Instrument, Plugins and Audio pages are verified by server-rendered DOM snapshots (`ihub-ui-pages`) and the Grid route by a native `--snapshot` PNG. The Instrument, Plugins and Audio pages are not captured as native web-view frames, and no browser or tablet has clicked through them.
- why: The page harness renders React under `react-dom/server`; the native snapshot test opens one route. Driving the embedded WebView route by route would need a UI automation driver that is not installed.
- human must: open `./build/engine/instrument-hub`, visit `/grid`, `/instrument`, `/plugins` and `/audio` in the embedded view, confirm layout, scrolling and 44 px touch targets, and capture a native frame per page if a pixel record is needed.
- workaround: `ihub-ui-pages` validates the page markup and writes the four snapshots; `ihub-ui-snapshot` validates the native 1200x800 Grid frame.
- note: B009's "eight slots" note is stale after T118. The composed session now owns all 64 slots and every pad arms audible audio, as `ihub-e2e` proves with the click/pad/LED/audio record.

## B011 · Autopilot blocked on T137
- task: T137
- severity: hard
- status: open
- what: crashed: Error: getaddrinfo EAI_AGAIN api.deepseek.com
- why: see the session log in `.autopilot/T137.log`
- human must: read the log for T137
- workaround: none recorded

## B012 · No JACK server to verify the native JACK path
- task: T206
- severity: hard
- status: **resolved 2026-11-03**
- what: The machine had `libjack` but neither `jackd` nor PipeWire's JACK server, so native JACK could never list a device and the open path could not be verified.
- why: `jackd` was not installed, no JACK socket existed under `/run/user/1000`, and the `pipewire-jack` module was absent; JUCE's JACK type therefore reported zero devices.
- human did: installed `pipewire-jack` 1.2.7-1ubuntu5.1 with apt.
- verified: the app launched through `scripts/run.sh` (which wraps the binary in `pw-jack`) lists 11 JACK devices including `Steinberg UR22mkII  Pro`; `POST /audio` switched to it and the app reports `open: true`, 48 kHz / 128 frames, 6.67 ms, 0 new xruns over 12 s (5 at start-up) and 0.33% load; `pw-link` shows the `JUCEJack` client wired to the UR22 playback ports.
- note: PipeWire's default quantum is 1024, so without `pw-metadata -n settings 0 clock.force-quantum 128` the app opens at 1024 frames / 53 ms. That setting is runtime-only; persist `default.clock.quantum = 128` under `~/.config/pipewire/pipewire.conf.d/` to keep it across restarts.
- workaround: ALSA stays the default fallback and remains playable; `ihub-device-manager-test` covers the no-server path, and T206 lists a running server's clients as device cards.

## B013 · Eight real Analog Lab V instances and editors
- task: T207
- severity: soft
- status: open
- what: The concurrent-instance regression covers eight sandboxed TestPlugin instances, editor open/close state capture, single-slot reload and restart under `DISPLAY=:0`. Wine-hosted Analog Lab V editors were exercised once through the API test (open, close, state capture), but eight real instances with eight different sounds and a save/restart have not been verified.
- why: A full eight-instance Analog Lab pass needs a licensed desktop session and enough time to choose eight presets; it cannot be scripted without driving the plug-in UI.
- human must: open `track-check.ihub` (eight Analog Lab V tracks), open each track's editor, choose a distinct preset in each, close every native window, then save and restart the project. Confirm all eight tracks keep their own sound, use `Reload plugin` on one track and confirm only that track restarts, and record the result in `docs/manual-analog-lab-64.md`.
- workaround: `ihub-session-concurrent-instances` proves the eight-instance lifecycle with TestPlugin; `ihub-plugin-editor-api` proves a real Analog Lab V editor close captures its state.
