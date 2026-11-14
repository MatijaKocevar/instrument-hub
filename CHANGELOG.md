# Changelog

All notable changes to Instrument Hub are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses semantic versioning.

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
