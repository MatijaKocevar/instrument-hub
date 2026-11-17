#pragma once

namespace ihub::sampler::instrument_detail {

inline constexpr int midiChannelMinimum = 1;
inline constexpr int midiChannelMaximum = 16;
inline constexpr int midiNoteMinimum = 0;
inline constexpr int midiNoteMaximum = 127;
inline constexpr int midiVelocityMinimum = 1;
inline constexpr int midiVelocityMaximum = 127;

inline constexpr float minimumGateReleaseMilliseconds = 0.1f;
inline constexpr float maximumGateReleaseMilliseconds = 100.0f;
inline constexpr float minimumChokeFadeMilliseconds = 0.1f;

inline constexpr int maximumMidiBytes = 3;

inline constexpr float monoPanSum = 0.70710678118f;

}  // namespace ihub::sampler::instrument_detail
