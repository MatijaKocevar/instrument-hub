#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

namespace AnalogLab
{

void pumpMessages();

void pumpFor (int milliseconds);

bool armAndWait (ihub::Session& session, int slot);

bool waitForAllWarm (ihub::Session& session, int slotCount);

ihub::SessionConfig makeConfig (const juce::String& bundlePath);

juce::File makeTempRoot();

bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory);

}
