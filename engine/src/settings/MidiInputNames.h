#pragma once

#include "settings/SettingsStore.h"

#include <vector>

namespace ihub::settings
{

bool sameMidiInput (const MidiInput& left, const MidiInput& right);

bool containsMidiInput (const std::vector<MidiInput>& inputs,
                        const MidiInput& candidate);

void addMidiInput (std::vector<MidiInput>& inputs, const MidiInput& input);

bool removeMidiInput (std::vector<MidiInput>& inputs, const MidiInput& input);

}
