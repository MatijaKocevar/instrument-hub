# Changelog

All notable changes to Instrument Hub are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses semantic versioning.

## [Unreleased]

### Added

- MIDI over WiFi: a paired Android phone can read a USB-MIDI keyboard with
  Chrome's Web MIDI support and forward it to the desktop as a normal MIDI
  input, over an HTTPS listener with a self-signed certificate.

## [0.3.0] - 2026-11-15

### Added

- Toast notifications for errors, warnings, success and information, with
  severity colours, expandable detail, repeat counting and individual or bulk
  dismissal.
- Sticky status cards for engine connectivity, instrument-host recovery,
  remote link loss, MIDI and patch faults, project write errors, plug-in scan
  errors and panic failures.
- Success feedback for project save and open, sound capture, plug-in scans,
  audio setup changes, latency measurement, pairing and panic.
- Stable unavailable panels with Retry actions for audio settings, MIDI
  inputs, plug-ins and insert chains when their data cannot be loaded.

### Changed

- Error messages and the recovering/reconnecting banners now surface as fixed
  toast cards instead of being inserted into the page.
- Status toasts deduplicate by key and pause their timers while hovered,
  focused or expanded.

### Fixed

- Error messages no longer move the page layout when they appear or disappear.
- Removed the remaining inline error blocks from the audio, MIDI, instrument,
  pairing, plug-in, project and remote views.

## [0.2.0] - 2026-11-14

### Added

- Built-in guitar **Drive** insert with six voicings: Overdrive, Blues, Rock,
  Distortion, Grunge and Fuzz. Each insert has Drive, Tone, Level and Cabinet
  controls, 4x oversampled waveshaping and a speaker-cabinet filter.
- Pass-through **Tuner** insert with YIN pitch detection and an analog gauge
  showing note, cents, frequency and in-tune state.
- Every active audio-interface input is now available in the source picker.
- Inline drive controls in the insert chain, with preset-specific defaults.
- Unique IDs for built-in inserts, so multiple pedals and hot-FX assignments
  work correctly.
- Insert renaming through the control API.

### Changed

- The Live track panel now spans the full instrument page height.
- FX parameter, bypass and name changes no longer bypass or rebuild the whole
  insert chain, keeping live knob changes and hot-FX switches glitch-free.

### Fixed

- The insert picker no longer opens outside the app window; it opens upward
  when needed and scrolls within the window.
- Removing or clearing an insert now clears its hot-FX assignment, so
  Launchpad pads no longer stay lit for effects that no longer exist.
- Stale hot-FX assignments are dropped from the state snapshot when their
  insert is missing.

## [0.1.0] - 2026-11-06

- Initial Linux release.
- Eight-track performance grid with VST3 hosting, sampler and live audio input.
- EQ, compressor, delay and reverb inserts, hot FX and Launchpad Mini MK3 control.
- Local HTTP + WebSocket control API and versioned `.ihub` projects.
