#pragma once

#include "settings/SettingsStore.h"

namespace ihub::settings::detail
{

void applyDocument (const juce::var& document, Settings& settings);

juce::var buildDocument (const juce::var& preserved, const Settings& settings);

}
