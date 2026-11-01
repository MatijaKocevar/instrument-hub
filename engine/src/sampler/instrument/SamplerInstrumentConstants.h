#pragma once

namespace ihub::sampler::instrument_detail
{

/** MIDI ranges accepted by `noteOn()`, `noteOff()` and the renderer. */
inline constexpr int midiChannelMinimum = 1;
inline constexpr int midiChannelMaximum = 16;
inline constexpr int midiNoteMinimum = 0;
inline constexpr int midiNoteMaximum = 127;
inline constexpr int midiVelocityMinimum = 1;
inline constexpr int midiVelocityMaximum = 127;

/** Envelope fade bounds, in milliseconds. */
inline constexpr float minimumGateReleaseMilliseconds = 0.1f;
inline constexpr float maximumGateReleaseMilliseconds = 100.0f;
inline constexpr float minimumChokeFadeMilliseconds = 0.1f;

/** A MIDI event never carries more than three bytes in this instrument. */
inline constexpr int maximumMidiBytes = 3;

/** -3 dB, used when a stereo voice is folded into a mono output. */
inline constexpr float monoPanSum = 0.70710678118f;

} // namespace ihub::sampler::instrument_detail
