#pragma once

#include "Session.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <string>

namespace ApiTest::PluginEditorApi
{

bool editorOpenInState (const juce::var& state, int slot);

bool waitForWarm (ihub::Session& session, int slot, int timeoutMs);

std::string patchSourceJson (const juce::String& path, const std::string& name);

juce::File findAnalogLabBundle();

}
