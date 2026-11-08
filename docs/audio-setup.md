# Audio setup wizard

Audio & MIDI → **Analyze & recommend** probes the connected soundcard and proposes
the lowest stable buffer. The dialog lists every probed setting, its latency
budget, its xrun count and the recommended row; **Apply** opens that setting and
stores it as the profile for the interface.

## What is probed

- 64, 96 and 128 frames.
- Every sample rate the device reports, with the current rate first (at most
  four rates, so a probe stays bounded).
- Device xruns after a short settle window.
- The latency budget used by `GET /audio`: one buffer plus device latency plus
  the FX-chain latency. The measured device round trip is shown as well when
  the selected device has an active input and a physical output-to-input cable.

A probe reopens the device several times, so audio drops out briefly. The
original device and format are restored before the result is shown. Applying
the recommendation is a separate action.

## Stability rule

The recommendation is the lowest buffer with zero xruns. If every buffer is
stable the lowest buffer wins; if none is stable the wizard reports the setting
with the fewest xruns and says so in the tradeoff text. Smaller buffers reduce
latency but leave the callback less time, so they are the first to xrun.

## Profiles and scope

Audio device type, device name, sample rate, buffer size and channel map are
global. Only FX-chain latency is per track. A wizard profile is keyed by the
soundcard, not by a track: the ALSA `Rubix22` and the JACK `Rubix22 Pro` share
one `rubix22` key, so either name picks up the saved 96-frame setting.

The profile is stored in `settings.json` under `audio.profiles` with its
device, format, channels, profile label and measured latency/xruns. On startup
the saved device is opened with its profile; selecting an interface later also
reuses its profile when the request does not specify a format.

## Linux pro-audio prerequisite

For a class-compliant USB interface the low-latency path is JACK on PipeWire.
Put the card in the `pro-audio` profile before starting the engine, then select
the `… Pro` JACK client (for example `Rubix22 Pro`):

```bash
pactl set-card-profile alsa_card.usb-Roland_Rubix22-00 pro-audio
pw-jack ./build-release/engine/instrument-hub --serve
```

The card name comes from `pactl list cards`. The wizard records this path as
the `pro-audio` profile when the selected device is JACK or its name ends in
`Pro`; it does not edit PipeWire configuration or require `sudo`.

## Legacy ALSA Loopback

The Loopback card remains a normal ALSA device. It can be probed, applied and
stored like any other interface, and the existing internal measurement source
still uses the ALSA Loopback.
