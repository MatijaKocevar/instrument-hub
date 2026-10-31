# Launchkey Mini MK3 protocol (superseded)

> **Superseded.** Instrument Hub drives the **Novation Launchpad Mini MK3**, not
> the Launchkey Mini MK3. The current controller reference is
> [`docs/launchpad-mini-mk3.md`](launchpad-mini-mk3.md).
>
> The `LaunchkeyProtocol` / `LaunchkeyDriver` modules and their simulator tests
> remain in the tree as history from tasks T040–T044. The application does not
> start them, and no task depends on the Launchkey behaviour.

For the record, the old module targeted the Launchkey MK3 programmer's
reference:

- SysEx header `F0 00 20 29 02 0F`, product/model `0F`.
- DAW port for modes, session pads, LEDs and encoders; MIDI port for keys.
- 16 pads in an 8×2 grid (notes 36..51), eight encoders, 25 keys.
- Session-mode frame and an Instrument-Hub-only XOR-7 checksum.

None of that describes the shipping surface: the Launchpad has 64 grid pads
(notes 11..88), no keys and no encoders. Use the Launchpad document.
