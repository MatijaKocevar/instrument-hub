# Settings persistence

Design for keeping app-level settings across launches. Queued as
`ROADMAP.md` T188–T192 and `BUGS.md` BUG-006.

## Problem

Only the set file and the plugin inventory survive a restart. Every other
setting is process memory:

| Setting | Today | Source |
| --- | --- | --- |
| audio device type, name, rate, buffer | lost on restart | `app/SessionSetup.cpp` picks `--audio-device` or the first ALSA output |
| master gain, input gain, MIDI channel, metronome | lost on restart | `api/state/TransportState.h` |
| enabled MIDI inputs | lost on restart | `api/MidiBridge.cpp` |
| channel masks | lost on restart | derived from the opened device |

The audio case is the one that bites: picking the UR22 in `/audio` works
until the next launch, then the app opens PipeWire's ALSA plugin and the
latency readout jumps from ~5 ms to ~19 ms. This is a missing requirement,
not a regression: T053 only ever asked for the device to change *live*.

## Decisions

**D1 — machine settings live outside the set.** One file,
`<userApplicationDataDirectory>/Instrument Hub/settings.json`, next to the
existing `KnownPluginList.xml`. A `.ihub` set stays portable; the audio
device is a property of the machine, not of the music.

**D2 — precedence: CLI flag > settings file > built-in default.** An
explicit `--audio-device` / `--audio-type` is never overridden by the file,
and a CLI run does not rewrite the file.

**D3 — write only after a successful apply.** `POST /audio` that opens the
device persists the effective type/name/rate/buffer. A failed apply writes
nothing. Writes are atomic (temp file in the same directory + `rename`), so
a crash cannot leave a half file.

**D4 — a failed restore is not fatal and does not erase the preference.**
If the stored device is gone, busy, or rejects the stored rate, the app
falls back (device default, then first ALSA output) and reports the reason
in `GET /audio` as a `restore` object. The stored entry stays as written, so
plugging the interface back in restores it on the next launch.

**D5 — versioned and forward compatible.** The file carries
`"version": 1`; unknown keys are preserved on rewrite; a corrupt file is
renamed `settings.json.bad` and defaults are used instead of failing
startup.

**D6 — identities are names, not indices.** Devices by `(type, name)`; MIDI
inputs by `(client name, port name)`. The current MIDI id (`ALSA:4:1`) is a
numeric ALSA client/port pair and changes across reboots, so it must never
be the persisted key.

**D7 — one writer, off the audio thread.** Startup restore and the `/audio`
handler share one `SettingsStore`; it is only touched from the message /
HTTP thread. The audio callback never reads or writes it.

## Schema (v1)

```json
{
  "version": 1,
  "audio": {
    "deviceType": "ALSA",
    "deviceName": "Steinberg UR22mkII, USB Audio; Direct hardware device without any conversions",
    "sampleRate": 48000,
    "bufferSize": 128
  },
  "mixer": { "masterGain": 0.8, "inputGain": 1.0, "midiChannel": 1, "metronome": false },
  "midi": { "enabledInputs": [ { "client": "Keystation Mini 32 MK3", "port": "Keystation Mini 32 MK3 MIDI 1" } ] }
}
```

## Restore flow

1. Load `settings.json`; absent or corrupt → defaults (D5).
2. `SessionSetup` applies CLI overrides on top (D2).
3. Open the stored device. Missing or failed → fall back per D4 and record
   `restore.fallback` + `restore.reason`.
4. Nothing is written during restore.

## Out of scope

- Per-set or per-song device profiles; named device profiles.
- Routing inside JACK/PipeWire (only the selected *device* is stored).
- Rewriting the set format or moving mixer values into `.ihub`.

## Tasks

| Task | Scope |
| --- | --- |
| T188 | `SettingsStore`: load/save, schema v1, atomic write, corrupt recovery, unknown-key preservation. Unit tests only, no wiring. |
| T189 | Audio device persistence: restore at startup, save on successful apply, CLI precedence, fallback reason in `/audio`. |
| T190 | Mixer settings through the same store. |
| T191 | Enabled MIDI inputs by `(client, port)` name. |
| T192 | UI: show the fallback reason and offer "forget this device". Snapshot test. |

Each task leaves `scripts/test.sh` green and touches one concern.
