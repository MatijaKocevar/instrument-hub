#include "State.h"
#include "Fixtures.h"

#include "instruments/PluginFx.h"

#include <iostream>
#include <string>

namespace PluginFxTest
{

void runStateRoundTrip (juce::AudioPluginFormatManager& manager,
                        const juce::PluginDescription& description)
{
    std::cout << "\n-- plugin state round-trips --" << std::endl;

    ihub::plugins::PluginFx source;
    juce::String error;

    check (source.load (manager, description, kSampleRate, kBlockSize, error),
           "the state source loads");

    if (! source.isLoaded())
        return;

    check (source.setParameterValue ("State Marker", kStateMarker),
           "State Marker is set");
    check (source.setParameterValue ("Gain", kStateGain),
           "Gain is changed before saving");
    check (source.setParameterValue ("Frequency", kStateFrequency),
           "Frequency is changed before saving");
    check (source.setParameterValue ("Latency", kLatencyMs / 20.0f),
           "Latency is changed before saving");

    const auto state = source.getStateInformation();
    check (state.getSize() > 0, "getStateInformation produces a non-empty blob");

    ihub::plugins::PluginFx restored;
    check (restored.load (manager, description, kSampleRate, kBlockSize, error),
           "a second FX instance loads");

    if (! restored.isLoaded())
        return;

    check (restored.setStateInformation (state),
           "setStateInformation accepts the blob");

    check (nearlyEqual (restored.getParameterValue ("State Marker"), kStateMarker),
           "State Marker survives the state round-trip");
    check (nearlyEqual (restored.getParameterValue ("Gain"), kStateGain),
           "Gain survives the state round-trip");
    check (nearlyEqual (restored.getParameterValue ("Frequency"), kStateFrequency),
           "Frequency survives the state round-trip");
    check (nearlyEqual (restored.getParameterValue ("Latency"), kLatencyMs / 20.0f),
           "Latency survives the state round-trip");
}

}
