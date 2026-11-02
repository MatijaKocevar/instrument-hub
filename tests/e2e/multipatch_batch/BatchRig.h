#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

namespace MultiPatch
{

void pumpMessages();
bool armAndWait (ihub::Session& session, int slot);
ihub::SessionConfig makeBatchConfig (const juce::String& bundlePath, int batchIndex);
bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory);

}
