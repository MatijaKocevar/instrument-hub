# Launchpad Mini MK3 protocol and mapping

Reference for the controller stack in `engine/src/controller/`:

- `LaunchpadProtocol.{h,cpp}` — pure protocol data and frame builders.
- `LaunchpadDriver.{h,cpp}` — MIDI I/O, handshake, pad/CC decoding, LEDs.
- `Mapping.{h,cpp}` — `shared/set.yaml`, pad/scene/bank routing, hot reload.

Nothing here runs on the audio thread. The driver has no audio knowledge; a
pad press becomes a slot number through the mapping, and the slot number is
published to the session with a lock-free arm request.

This is the only controller stack in the tree; there is no second device path.

## Sources

- Novation, *Launchpad Mini [MK3] Programmer's Reference Manual* (SysEx header,
  programmer mode, LED lighting, palette, layout selection). Novation
  documentation downloads, product "Launchpad Mini MK3".
- On-hardware capture recorded in `LaunchpadProtocol.h`: pads report notes
  11..88 on the `MI` port, presses are Note On, releases are Note On velocity 0,
  and the `MI` port is the only port that accepts LED Note On messages. The
  `DA` port carries no pad or LED data.

## MIDI ports

The Mini MK3 exposes two USB MIDI ports. The driver deliberately selects the
**MI** port in both directions:

- `Launchpad Mini MK3 MI` (or `LPMiniMK3 MI`): grid notes, top row, right
  column, LEDs.
- `Launchpad Mini MK3 DA`: DAW/Live behaviour; inert for this application.

`LaunchpadDriver::findDevice()` matches the name fragment
`"launchpad mini mk3"`, prefers a port whose name ends in `MI` (checked as
`mk3 mi` / `lpminimk3 mi`, not the `mi` inside `Mini`), and never falls back to
the `DA` port. There is no virtual Launchpad on the ALSA sequencer: the tests
inject MIDI bytes into `LaunchpadDriver::injectMidiMessage()` and capture LED
bytes with `setMessageSink()`, which uses the same decoder and encoder as the
MIDI thread.

## SysEx envelope and programmer mode

Every message starts with the Novation header:

```
F0 00 20 29 02 0D   <command>   <payload...>   F7
```

- `F0` start of exclusive
- `00 20 29` Novation EMS manufacturer ID
- `02` Launchpad product family
- `0D` Launchpad Mini MK3 model byte (Launchpad X is `0C`, Pro MK3 is `0E`)
- `F7` end of exclusive

All payload bytes are 7-bit. Instrument Hub sends no checksum.

On connect `LaunchpadDriver::sendHandshake()` sends, in order:

| Purpose | Bytes |
| --- | --- |
| Enter programmer mode | `F0 00 20 29 02 0D 0E 01 F7` |
| External LED feedback (internal off) | `F0 00 20 29 02 0D 0A 00 01 F7` |
| Wake from LED sleep | `F0 00 20 29 02 0D 09 01 F7` |

Leaving programmer mode on disconnect sends `F0 00 20 29 02 0D 0E 00 F7`, so
the surface returns to Live mode instead of staying dark. A received
programmer-mode frame sets `hasReceivedHandshakeEcho()`.

## Grid

The MK3 grid is 8×8. Pads report notes 11..88; the note is
`10 * row + column` with both digits in 1..8.

Instrument Hub pad index is row-major from the **bottom-left**:

- pad 0 is note 11 (bottom-left), pad 7 is note 18 (bottom-right),
- pad 56 is note 81, pad 63 is note 88 (top-right).

`LaunchpadProtocol::padIndexForNote()` rejects anything that is not a two-digit
grid note, so a stray keyboard note cannot arm a slot.

A pad is lit by Note On on the same channel and note, with a palette velocity:

```
90 <note> <velocity>      static
91 <note> <velocity>      flashing
92 <note> <velocity>      pulsing
```

## Top row, right column and logo

With no keys and no encoders on the Mini MK3, the surrounding buttons carry the
control surface:

| Area | CC | Meaning in Instrument Hub |
| --- | --- | --- |
| Top row, left to right | 91..98 | Macro page / bank 0..7 |
| Right column, top to bottom | 89, 79, 69, 59, 49, 39, 29, 19 | Scene 0..7 |
| Logo | 99 | Decoded; no mapping yet |

Button LEDs use the same palette velocity on a CC message:
`B0 <cc> <velocity>` (with `B1`/`B2` for flashing/pulsing).

`shared/set.yaml` declares eight `banks` (Filter, Resonance, Space, Delay,
Drive, Tone, Volume, Pan) and eight `scenes` (Vox, Percussion, FX, Lead, Pads,
Bass, Drums, Keys). A scene names the family row it lines up with; it is a
controller selection, not a graph change.

## LED policy

The grid mirrors `/state`: every unarmed patch lights its `colorIndex`
palette colour, the armed patch is bright red, a patch that is warming is
yellow, and a load error flashes red.

| State | Colour | Velocity | When |
| --- | --- | --- | --- |
| `idle` | patch palette | 9..57, no red | unselected patch |
| `warming` | Yellow | 13 | card is loading a cold patch |
| `armed` | Red | 5 | the armed patch |
| `error` | Red, flashing | 5 (`0x91`) | slot could not be loaded |

The generic `PadState` feedback used by the controller scripts keeps its
legacy mapping (idle off, warming yellow, active green, error red); the app
uses the patch-aware `LaunchpadDriver::setPatchLed()` policy above. Button
LEDs use the same palette velocity on a CC message.

LED state is stored per pad/button, so `refreshLeds()` can repaint the whole
surface after a reconnect without replaying controller history. The app polls
`/state` at 10 Hz and repaints only changed LEDs, so an HTTP colour edit reaches
the hardware without a reconnect.

## Mapping and the composed session

`shared/set.yaml` is hot-reloadable. A pad press resolves
`pad -> family -> instrument -> slot`, and the `armSlot` listener publishes
that slot to `Session::armSlot()`. The audio graph is never rewired: all slots
are already wired in parallel and arming only changes which slot is faded up
over 3 ms.

The shipped mapping declares 64 pads and 64 slots (8 families × 8 patches),
and the composed application now builds all **64** logical slots. Every pad
0..63 resolves to its own slot and arms audible audio; the warm pool keeps the
armed slot plus its neighbours instantiated, so a cold pad reports `warming`
before it crossfades in.

### Grid page and the same-patch contract

The Grid page and the Launchpad are two views of the same arm:

- clicking a grid cell calls `POST /arm` with the patch slot and opens
  `/instrument?slot=N`;
- pressing the matching Launchpad pad goes `pad -> mapping -> Session::armSlot`;
- both paths publish the same `armedSlot`, the same non-silent source, and the
  same LED (armed pad red, other pads their patch colour).

`ihub-e2e` proves this headlessly: it arms slot 10 through the HTTP route,
moves away with pad 0, re-arms slot 10 with the physical pad path, renders both
windows non-silent, and captures the LED stream (armed velocity 5 / static,
idle patch velocity 13). See `ihub-e2e-pages-record.txt` beside the test
binary. `ihub-ui-pages` renders the Grid, Instrument, Plugins and Audio pages
and checks the snapshots in `tests/artifacts/`.

## Hardware status

No Launchpad Mini MK3 is attached to the build machine. The protocol tables and
the full pad → mapping → LED path are covered by:

- `ihub-launchpad-protocol` — frame bytes, grid notes, palette.
- `ihub-launchpad-mapping` — all 64 pads, CC 91..98 banks, CC 89..19 scenes.
- `ihub-launchpad-driver` — scripted event sequence and the exact LED stream.
- `ihub-ui-pages` — Grid, Instrument, Plugins and Audio page snapshots.
- `ihub-e2e` — Grid click plus pad event through the mapping into the composed
  session, non-silent audio rendered and captured on the ALSA Loopback, and
  the armed-red / patch-colour LED stream.

Physical verification (programmer-mode entry, real LED colours, font/brightness,
firmware quirks) is tracked in `plan/BLOCKERS.md` as **B009**.
