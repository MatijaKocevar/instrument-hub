#include "Check.h"
#include "analog_lab/Constants.h"
#include "analog_lab/SessionRig.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string>

namespace AnalogLab
{
using E2E::check;


/** Pumps the JUCE message loop once. JUCE posts plugin creation there, so the
    loop must keep running while a warm load is in flight. */
void pumpMessages()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

void pumpFor (int milliseconds)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (milliseconds);

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
        pumpMessages();
}

bool armAndWait (ihub::Session& session, int slot)
{
    session.armSlot (slot);

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (kArmTimeoutMs);

    if (session.getWarmState (slot) == ihub::SessionWarmState::active)
        return true;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        pumpMessages();

        if (session.getWarmState (slot) == ihub::SessionWarmState::active)
            return true;
    }

    check (false,
           "slot " + std::to_string (slot) + " did not become active within "
               + std::to_string (kArmTimeoutMs) + " ms (state: "
               + session.getWarmStateName (slot) + ")");
    return false;
}

bool waitForAllWarm (ihub::Session& session, int slotCount)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (kWarmTimeoutMs);

    for (;;)
    {
        bool allWarm = true;

        for (int slot = 0; slot < slotCount; ++slot)
            allWarm = session.isSlotWarm (slot) && allWarm;

        if (allWarm)
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            break;

        pumpMessages();
    }

    std::string states;

    for (int slot = 0; slot < slotCount; ++slot)
    {
        if (! states.empty())
            states += ", ";

        states += "slot " + std::to_string (slot) + "="
                + session.getWarmStateName (slot);
    }

    check (false, "not every Analog Lab patch warmed within "
                      + std::to_string (kWarmTimeoutMs) + " ms (" + states + ")");
    return false;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = kNeighbourCount;
    config.initialSlot = 0;

    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (kSlots);

    for (int slot = 0; slot < kSlots; ++slot)
    {
        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
        slotConfig.kind = ihub::SessionSlotKind::plugin;
        slotConfig.path = bundlePath;
        slotConfig.pluginFormat = "VST3";
        slotConfig.pluginName = "Analog Lab V";
        slotConfig.pluginManufacturer = "Arturia";
        slotConfig.instrumentKind = "plugin";
        slotConfig.name = slot == 0 ? "Analog Lab A" : "Analog Lab B";
        slotConfig.patch = slot == 0 ? "analog-lab-a" : "analog-lab-b";
        slotConfig.patchName = slotConfig.name;
        slotConfig.family = slot == 0 ? "keys" : "pads";
        slotConfig.familyName = slot == 0 ? "Keys" : "Pads";
    }

    return config;
}

juce::File makeTempRoot()
{
    const juce::File approvedBase ("/tmp/opencode");
    const auto base = approvedBase.isDirectory()
                          ? approvedBase
                          : juce::File::getSpecialLocation (
                                juce::File::tempDirectory);

    return base.getChildFile ("ihub-t131-analog-"
                              + juce::String::toHexString (
                                    juce::Random::getSystemRandom().nextInt64()));
}

bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory)
{
    session.setPluginStateDirectory (stateDirectory);

    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "FAIL analog-lab-state: session build failed: " << error
                  << std::endl;
        return false;
    }

    return true;
}

} // namespace AnalogLab
