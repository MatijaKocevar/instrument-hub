#pragma once

#include "settings/SettingsStore.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace ihub::settings::detail {

void readAudioSettings(const juce::var& value, AudioSettings& audio);

void writeAudioSettings(juce::DynamicObject& object,
                        const AudioSettings& audio);

}  // namespace ihub::settings::detail
