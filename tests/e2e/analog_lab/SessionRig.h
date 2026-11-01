#pragma once

#include "Session.h"

#include <juce_core/juce_core.h>

namespace AnalogLab
{

void pumpMessages();

void pumpFor (int milliseconds);

/** Calls `armSlot(slot)` and pumps until that slot is active or the 5 s
    deadline expires. On timeout the failure names the observed warm state. */
bool armAndWait (ihub::Session& session, int slot);

/** Waits until every slot's instance is resident, with the same bounded pump.
    This keeps a background load from running while the audio probe renders. */
bool waitForAllWarm (ihub::Session& session, int slotCount);

ihub::SessionConfig makeConfig (const juce::String& bundlePath);

juce::File makeTempRoot();

bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory);

} // namespace AnalogLab
