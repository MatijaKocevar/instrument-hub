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
- status: open
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
