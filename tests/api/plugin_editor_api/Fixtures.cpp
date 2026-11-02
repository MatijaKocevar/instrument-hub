#include "Fixtures.h"

#include "../JsonSupport.h"

#include <juce_events/juce_events.h>

#include <cstdlib>
#include <string>

namespace ApiTest::PluginEditorApi
{

bool editorOpenInState (const juce::var& state, int slot)
{
    return boolProperty (patchForSlot (state, slot), "editorOpen");
}

bool waitForWarm (ihub::Session& session, int slot, int timeoutMs)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (session.isSlotWarm (slot))
            return true;

        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
    }

    return session.isSlotWarm (slot);
}

std::string patchSourceJson (const juce::String& path, const std::string& name)
{
    return "{\"kind\":\"plugin\",\"name\":\"" + name + "\",\"path\":\""
           + path.toStdString() + "\",\"pluginName\":\"" + name + "\"}";
}

juce::File findAnalogLabBundle()
{
    if (const auto* env = std::getenv ("IHUB_ANALOG_LAB_VST3");
        env != nullptr && *env != '\0')
    {
        const juce::File candidate { juce::String (env) };
        return candidate.exists() ? candidate : juce::File();
    }

    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const juce::Array<juce::File> candidates {
        juce::File ("/usr/lib/vst3/Analog Lab V.vst3"),
        juce::File ("/usr/local/lib/vst3/Analog Lab V.vst3"),
        home.getChildFile (".vst3/Analog Lab V.vst3"),
        juce::File ("/usr/lib/vst3/vstbridge/Analog Lab V.vst3"),
        home.getChildFile (".vst3/vstbridge/Analog Lab V.vst3"),
        juce::File ("/usr/lib/vst3/yabridge/Analog Lab V.vst3"),
        home.getChildFile (".vst3/yabridge/Analog Lab V.vst3"),
    };

    for (const auto& candidate : candidates)
        if (candidate.exists())
            return candidate;

    return {};
}

}
