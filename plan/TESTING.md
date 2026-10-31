# Testing Without Hardware

Everything here runs on this machine. No controller, no interface, no plugins.

---

## Audio: ALSA loopback

`card 2: Loopback` is already loaded — playback device 0 feeds capture device 0.
Find it by name, never by index:

```bash
aplay -l | grep -i loopback
```

Rules:

- Prefer `hw:Loopback,0,0` for playback and `hw:Loopback,1,0` for capture.
- 48 kHz, 128 frames unless the task says otherwise.
- If silence comes back, check that nothing else holds the capture side
  (`fuser -v /dev/snd/pcmC2D0p`), and remember loopback has 8 subdevices —
  a previous test may still own one.

Round-trip latency: play an impulse, capture it, cross-correlate, report
`lag_samples * 1000 / sample_rate` ms. Expect roughly `2 * buffer / sr`
plus a small constant.

Xruns: time `processBlock` against `buffer / sr`. On the loopback this is
a *lower bound* — real hardware is slower. Record that caveat.

---

## MIDI: virtual Launchkey

ALSA sequencer (`/dev/snd/seq`) is present. The simulator creates a port
literally named `Launchkey Mini MK3 SIM` so the driver's device matching
can be tested unmodified.

Fixture format (JSON):

```json
{ "events": [
  { "t": 0,    "type": "sysex", "hex": "F0 00 20 29 02 0F 00 ... F7" },
  { "t": 120,  "type": "noteon", "ch": 1, "note": 36, "vel": 100 },
  { "t": 360,  "type": "noteoff","ch": 1, "note": 36, "vel": 0 }
] }
```

Timestamps are milliseconds from start. The simulator also records every
message the app emits, so LED output can be compared against a golden file.

Anything the driver says is untested without this is not tested.

---

## Plugins: our own

`tests/plugins/TestPlugin` is a JUCE VST3 we build ourselves:

- a sine generator with a settable frequency
- a gain parameter
- a settable artificial latency (0–20 ms)
- a non-default parameter used to prove state round-trips

This removes all third-party dependency from CI. Optical plugins
(Analog Lab, EZdrummer) are only ever exercised by a human — see `BLOCKERS.md`.

---

## UI

- Headless-ish: `DISPLAY=:0` is available, so the real window can run.
- Screenshots come from inside JUCE (`Component::createComponentSnapshot`),
  exposed as `--snapshot out.png`. Do not shell out to a desktop screenshot tool.
- A snapshot is valid only if it is non-blank and the expected widget count is
  found. "It rendered" is not a test; assert on pixels or on the DOM via the
  web view's JS bridge.
- The React app has a headless snapshot test that needs no browser at all:
  `cd ui && pnpm test:snapshot` renders the real tree with `react-dom/server`
  against a fixture API, asserts the widget count and content, and writes the
  markup to `tests/artifacts/`. Pass `--api=http://127.0.0.1:18080` to the
  built harness to render live engine state from `instrument-hub --serve`.

---

## Determinism

- Render to file and compare against goldens with a tolerance, never exact bytes,
  for anything touching floating point.
- Randomised tests must seed from a constant.
- Audio tests must not depend on wall-clock timing.

---

## Commands you will use constantly

```bash
scripts/build.sh                       # configure + build
./build/engine/instrument-hub --selftest
./build/engine/instrument-hub --list-devices
./build/engine/instrument-hub --measure-latency
./build/engine/instrument-hub --snapshot out.png
./build/engine/instrument-hub --rt-strict
./build/tests/ihub-selftest --midi-probe
ctest --test-dir build --output-on-failure
```

## Realtime safety harness (T014)

`RtGuard` is compiled into Debug and RelWithDebInfo builds. It replaces
`operator new`, wraps `malloc`/`pthread_mutex_*` at link time and installs a
`juce::Logger` probe, so allocation, locking or logging inside a
`processBlock()` scope is counted and — in strict mode — aborts with a
diagnostic naming the violation.

```bash
# Normal path: must print PASS and exit 0
./build/engine/instrument-hub --rt-strict

# Deliberate violations: each must abort (exit 134 / SIGABRT)
./build/engine/instrument-hub --rt-strict --rt-inject-allocation
./build/engine/instrument-hub --rt-strict --rt-inject-lock
./build/engine/instrument-hub --rt-strict --rt-inject-log

# Same checks, fork-isolated, as part of the normal test run
./build/tests/ihub-rt-guard-test
```

Release builds compile the harness out by default; configure with
`-DIHUB_ENABLE_RT_GUARD=ON` to keep it, or `OFF` to drop it from a development
build.
