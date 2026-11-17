#pragma once

#include <cstdint>

namespace Api {

struct SettingsState {
  double masterGain = 0.8;
  double inputGain = 1.0;
  int midiChannel = 1;
  bool metronome = false;
};

struct TransportState {
  bool playing = false;
  double bpm = 120.0;
  double ppq = 0.0;
  std::int64_t positionSamples = 0;
};

}  // namespace Api
