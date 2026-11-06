# Changelog

## 0.1.0 — 2026-11-06

First Linux release.

### Added

- Eight-track performance grid with an instrument, FX chain, two hot-FX
  switches and a five-step volume fader per track.
- Launchpad Mini MK3 programmer-mode LEDs, track pads, hot-FX toggles and
  volume steps.
- Generic MIDI keyboards and controllers, including a learn path for 64-pad
  grids.
- VST3 scanning and page-scoped plugin hosting with per-patch state capture,
  editor windows and automatic host recovery.
- Sampler instruments (WAV/AIFF/FLAC, velocity layers, round-robin and choke
  groups), live audio-input tracks, and built-in EQ, compressor, delay and
  reverb inserts.
- `.ihub` projects with autosave, recent projects, per-patch plugin state and
  migration from the original 64-patch family sets.
- Audio setup wizard, per-interface profiles, latency measurement and xrun
  reporting for ALSA, JACK and PipeWire.
- Local HTTP/WebSocket API, token pairing, LAN mode and the tablet remote.

### Packaging

- Linux x86_64 AppImage with `instrument-hub`, `ihub-scanner` and
  `ihub-sandbox`.
- The blank track template is embedded in the application, so New Project
  works from a clean AppImage install without a repository checkout.
- `SHA256SUMS` is published with each release.

### Known limitations

- Linux is the only published platform for this release; Windows and macOS
  packaging are deferred.
- The AppImage is unsigned.
- Physical Launchpad, audio-interface, tablet-remote and third-party plugin
  validation is still outstanding; see `plan/BLOCKERS.md`.
