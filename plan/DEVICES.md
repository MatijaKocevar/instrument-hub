# Audio device list

Design for a usable device picker. Queued as `ROADMAP.md` T193–T195 and
`BUGS.md` BUG-007.

## Problem

`DeviceEnumeration` asks JUCE's ALSA type for every PCM the ALSA hints
return. On this machine that is ~60 entries for two cards: `hw`, `plughw`,
`front`, `dmix`, `dsnoop`, `surround*`, `iec958`, rate converters, the two
Loopback sides, JACK's PCM and PipeWire's `pulse` PCM. Picking most of them
either fails or silently lands on a deep-buffer plugin.

Worse, the entries that look most correct are the ones that fail. With the
app playing through PipeWire, the UR22 `hw` PCM is held by PipeWire; JUCE
cannot probe its channels, the snapshot reports zero channels, and `apply`
then answers a device-open problem with `outputChannels must select at least
one channel`. With PipeWire suspended the same PCM opens.

## Decisions

**D1 — one entry per card, plus one system entry.** The list is built from
cards (`snd_card_next` / `snd_pcm_info`), not from PCM hint strings:
`Steinberg UR22mkII`, `HD-Audio Generic (analog)`, `HD-Audio Generic (HDMI)`,
and `System (PipeWire)`. `VirMIDI`, `Loopback`, Launchpad and Keystation are
not outputs and never appear.

**D2 — the name maps to a family, not a string.** Selecting a card tries, in
order, the PCM that needs no conversion (`hw`), the plug variant
(`plughw`/`front`), and the card's `default`; the first that opens wins and
is recorded. The preference file still stores the friendly card name, so a
machine that renumbers cards keeps working.

**D3 — never list what cannot open.** Each candidate is probed once at
enumeration with JUCE's own format set (float, then S32/S24/S16). A PCM that
cannot be opened is omitted; if the whole card is busy, the card is listed
once as `unavailable` with the reason (`in use by PipeWire`) instead of five
broken duplicates.

**D4 — errors say what happened.** `EBUSY` becomes "the device is in use by
another process (PipeWire)", a missing card becomes "the device is not
connected", and channel problems stay channel problems. The channel error is
only produced when the user actually sent a channel list.

**D5 — channel defaults do not depend on a probe.** A device switch defaults
to the first two output channels and every input channel the device reports;
an empty probe result is a failed probe, not a zero-channel device.

**D6 — test devices are opt-in.** The ALSA Loopback stays out of the normal
list and appears with `--show-test-devices` (tests and diagnostics pass it).
The test rig keeps using the Loopback by explicit name, unchanged.

**D7 — advanced escape hatch.** A raw ALSA PCM list is available behind an
"Advanced" toggle in the UI and `--raw-devices` on the CLI, so a user who
needs `iec958` or a custom PCM string is not locked out.

## Tasks

| Task | Scope |
| --- | --- |
| T193 | Card-based enumeration, probe/filter, friendly names, Loopback gating, `--show-test-devices` / `--raw-devices`. |
| T194 | Switch reliability: family resolution, channel defaults, EBUSY/not-connected errors, regression tests. |
| T195 | UI: friendly picker, unavailable state with reason, advanced toggle, snapshot test. |

BUG-007 is fixed when selecting the UR22 from the normal list opens it at
48 kHz/128 in one request, with no channel payload.
