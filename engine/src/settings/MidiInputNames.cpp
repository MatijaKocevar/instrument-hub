#include "settings/MidiInputNames.h"

#include <algorithm>

namespace ihub::settings {

bool sameMidiInput(const MidiInput& left, const MidiInput& right) {
  if (left.port.isEmpty() || right.port.isEmpty() || left.port != right.port)
    return false;

  if (left.client.isEmpty() || right.client.isEmpty()) return true;

  return left.client == right.client;
}

bool containsMidiInput(const std::vector<MidiInput>& inputs,
                       const MidiInput& candidate) {
  return std::any_of(inputs.begin(), inputs.end(),
                     [&candidate](const MidiInput& input) {
                       return sameMidiInput(input, candidate);
                     });
}

void addMidiInput(std::vector<MidiInput>& inputs, const MidiInput& input) {
  if (!containsMidiInput(inputs, input)) inputs.push_back(input);
}

bool removeMidiInput(std::vector<MidiInput>& inputs, const MidiInput& input) {
  const auto previousSize = inputs.size();

  inputs.erase(std::remove_if(inputs.begin(), inputs.end(),
                              [&input](const MidiInput& candidate) {
                                return sameMidiInput(candidate, input);
                              }),
               inputs.end());

  return inputs.size() != previousSize;
}

}  // namespace ihub::settings
