# Launchkey Mini MK3 protocol

Reference for the pure protocol layer in
`engine/src/controller/LaunchkeyProtocol.{h,cpp}`. Nothing here performs MIDI
I/O; `LaunchkeyProtocolTest` checks every table and frame documented below.

## Sources

- Novation, *Launchkey MK3 Programmer's Reference Manual*:
  <https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launchkey_MK3_Programmers_Reference.pdf>
  - SysEx header, DAW mode, pad layouts, colour palette, encoder CCs.
- Novation, *Launchkey Mini MK3 User Guide 1.1*:
  <https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launchkey%20Mini%20MK3%20User%20Guide%201.1%20English%20EN.pdf>
  - 25-key range, 8x2 pad grid, pad modes.
- Ableton Live 11 `Launchkey_Mini_MK3` remote script (Novation palette table and
  Mini session-mode note indices), cross-checked against the reverse-engineered
  Mini MK3 implementation at <https://github.com/giezu/LaunchkeyMiniMK3>.

The public programmer's reference covers the Launchkey MK3 family (25/49/61).
The Mini MK3 shares the same SysEx header and product byte. Where the Mini
differs, it is called out below.

## MIDI ports

The device exposes two USB MIDI ports. The driver uses the **DAW** port:

- DAW in/out: modes, session pads, LEDs, encoders. This is the port the
  protocol layer targets.
- MIDI in/out: keys, touch strips, and Custom-mode controls.

## SysEx envelope

Every Launchkey MK3 SysEx message begins with the Novation header:

```
F0 00 20 29 02 0F   <command>   <payload...>   F7
```

- `F0` start of exclusive
- `00 20 29` Novation EMS manufacturer ID
- `02` product family
- `0F` product/model byte (Launchkey MK3 series)
- `F7` end of exclusive

All bytes between the header and `F7` are 7-bit (`0x00..0x7F`).

### Instrument Hub checksum

The published Launchkey MK3 messages do **not** carry a checksum. Instrument
Hub adds one only to its own reserved-command frames, so the documented
display/velocity/startup messages keep the exact vendor byte sequence. The
checksum lets the driver and the virtual Launchkey simulator reject a corrupted
reserved frame before it reaches hardware:

```
checksum = XOR(command, payload[0], payload[1], ...) & 0x7F
```

The checksum sits immediately before `F7`. Frames built with
`Launchkey::Checksum::None` (the default) omit it; only the session-mode frame
uses `Checksum::Xor7`.

### Session-mode frame (documented example)

The session-mode frame used by `Launchkey::sessionModeFrame()` is:

```
F0 00 20 29 02 0F 0C 01 0D F7
```

- Length: **10 bytes**
- Command: `0x0C`
- Payload: `0x01` (session layout)
- Checksum: `0x0C XOR 0x01 = 0x0D`

`LaunchkeyProtocolTest` asserts the exact length, every byte, the recomputed
checksum, and that a corrupted checksum / header / length is rejected.

> The Mini MK3 has no SysEx session-mode switch in the public reference. The
> working switch on current firmware is the note/CC pair below; the SysEx frame
> is an Instrument Hub reserved-command encoding and is not required to enter
> session mode. A driver sends the note/CC pair as well. Both are exposed by
> the module.

## Entering session mode

Documented sequence on the DAW port:

| Purpose | Bytes | Meaning |
| --- | --- | --- |
| Enter DAW mode | `9F 0C 7F` | Note On, channel 16, note 12, velocity 127 |
| Select session pads | `BF 03 02` | CC, channel 16, controller 3, value 2 |
| Select drum pads | `BF 03 01` | CC, channel 16, controller 3, value 1 |
| Leave DAW mode | `9F 0C 00` | Note On, channel 16, note 12, velocity 0 |

Enter DAW mode first; the pad layout only applies in DAW mode. Session mode is
the default pad layout after entering DAW mode, but the driver sends the layout
message explicitly so it can recover from a user changing modes.

## Pads

The Mini has 16 pads in an 8x2 grid.

Instrument Hub pad index (physical order):

- pads `0..7`: bottom row, left to right
- pads `8..15`: top row, left to right

Instrument Hub pad note map (the controller contract):

| Pad | Note | Pad | Note |
| --- | --- | --- | --- |
| 0 | 36 | 8 | 44 |
| 1 | 37 | 9 | 45 |
| 2 | 38 | 10 | 46 |
| 3 | 39 | 11 | 47 |
| 4 | 40 | 12 | 48 |
| 5 | 41 | 13 | 49 |
| 6 | 42 | 14 | 50 |
| 7 | 43 | 15 | 51 |

Note 36..51 maps to pad 0..15 and back; notes outside that range are not pads.
`padNoteForIndex()` / `padIndexForNote()` implement the table.

### Session LED note indices

The vendor addresses the session pads for colouring with different note
numbers: `96..103` for the top row (pads 8..15) and `112..119` for the bottom
row (pads 0..7). `sessionLedNoteForIndex()` exposes that mapping, and
`padIndexForNote()` accepts the 36..51 contract range only. A driver that
receives vendor session notes should translate `96..103 -> 8..15` and
`112..119 -> 0..7` before calling the pad-index API.

## LED colours

A pad LED is lit with a Note On whose channel selects the animation and whose
velocity selects a palette colour:

| Channel | Mode | Status byte |
| --- | --- | --- |
| 1 | static | `0x90` |
| 2 | flashing | `0x91` |
| 3 | pulsing | `0x92` |

Message: `<status> <session LED note> <palette velocity>`.
`Launchkey::padColourMessage()` uses `sessionLedNoteForIndex()` for the note,
because the vendor addresses the session LEDs with 96..119, not with the
36..51 trigger notes.

The palette has 128 slots (velocities `0..127`). The documented examples are
reproduced exactly:

| Colour | Velocity |
| --- | --- |
| Off | `0x00` (0) |
| Red | `0x05` (5) |
| Green | `0x13` (19) |
| Blue | `0x2D` (45) |

`Launchkey::palette` adds named entries from the same 128-entry table
(dark grey, grey, white, pink, orange, cream, mint, light blue, dark blue,
violet, purple, aqua, yellow), each with a unique velocity byte. The test
asserts uniqueness across the whole named palette, not just the four examples.

## Encoders

The 8 knobs are reported on the DAW port as Control Change on channel 16:

| Encoder | CC |
| --- | --- |
| 0 (leftmost) | 21 |
| 1 | 22 |
| 2 | 23 |
| 3 | 24 |
| 4 | 25 |
| 5 | 26 |
| 6 | 27 |
| 7 (rightmost) | 28 |

Status byte `0xBF`, value `0..127`. `encoderCCForIndex()` and
`encoderIndexForCC()` implement the table. Encoder -> macro routing is a
mapping concern (`set.yaml`), not part of this layer.

## Keyboard

The 25 keys span C2..C4, i.e. MIDI notes 48..72 inclusive (Novation's
convention is C3 = 60). `isKeyNote()` accepts exactly that range. Octave and
transpose buttons change the transmitted note without changing the physical
range, so the driver must not filter key notes by this range; it is exposed for
UI display and range validation only.

## What this layer does not do

- No MIDI port is opened, no message is queued, no clock is read.
- No mapping from pads/encoders to engine slots; that lives in `set.yaml`.
- No scene/clip state; Session-mode feedback from Ableton is out of scope.
