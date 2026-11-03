#pragma once

#include "project/SetFile.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace Api
{
class StateStore;
}

namespace ihub
{
class Session;
}

namespace ihub::app
{

bool captureProject (ihub::Session& session,
                     const Api::StateStore& state,
                     const project::SetFile& base,
                     const std::vector<int>& touchedSlots,
                     project::SetFile& destination,
                     juce::String& error);

}
