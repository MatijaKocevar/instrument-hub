#include "GainLatency.h"
#include "Fixtures.h"

#include "audio/FxChain.h"
#include "instruments/PluginFx.h"

#include <cmath>
#include <iostream>
#include <string>

namespace PluginFxTest
{

int runGainAndLatency (juce::AudioPluginFormatManager& manager,
                       const juce::PluginDescription& description)
{
    std::cout << "\n-- TestPlugin in FX mode applies gain and latency --" << std::endl;

    ihub::plugins::PluginFx fx;
    juce::String error;

    check (fx.load (manager, description, kSampleRate, kBlockSize, error),
           "PluginFx loads TestPlugin" + (error.isEmpty() ? std::string()
                                                          : " (" + error.toStdString() + ")"));

    if (! fx.isLoaded())
        return 1;

    check (fx.getName().contains ("TestPlugin"), "the wrapper reports the plugin name");
    check (fx.isFxMode(), "the plugin is in FX mode");
    check (fx.getNumInputChannels() == kNumChannels,
           "the main input bus is enabled at the engine's channel count");
    check (fx.getNumOutputChannels() == kNumChannels,
           "the main output bus matches the engine's channel count");

    check (fx.setParameterValue ("Gain", kGain), "Gain parameter exists");
    check (fx.setParameterValue ("Sine Level", 0.0f), "Sine Level parameter exists");
    check (fx.setParameterValue ("Latency", 0.0f), "Latency parameter exists");

    fx.prepare (kSampleRate, kBlockSize);
    check (fx.latencySamples() == 0, "a zero-latency plugin reports zero");

    AudioEngine::FxChain chain;
    check (chain.add (fx.getProcessor()) == 0, "the hosted processor is appended to the chain");
    check (fx.isInChain (chain), "the wrapper finds itself in the chain");

    chain.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (fx.getNumInputChannels() == kNumChannels,
           "chain preparation keeps the input bus enabled");

    juce::AudioBuffer<float> input (kNumChannels, kBlockSize);
    fillPattern (input);

    juce::AudioBuffer<float> output;
    output.makeCopyOf (input);

    juce::MidiBuffer midi;
    chain.process (output, midi);

    double gainError = 0.0;

    for (int channel = 0; channel < kNumChannels; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float expected = input.getSample (channel, i) * kGain;
            gainError = juce::jmax (
                gainError,
                std::abs (static_cast<double> (output.getSample (channel, i) - expected)));
        }

    check (gainError < 1.0e-6,
           "the chain output is the input scaled by the plugin's 0.5 gain");

    check (fx.setParameterValue ("Latency", kLatencyMs / 20.0f),
           "Latency is set to 5 ms");

    juce::AudioBuffer<float> latencyBlock (kNumChannels, kBlockSize);
    latencyBlock.clear();
    chain.process (latencyBlock, midi);

    fx.refreshLatency();
    chain.refreshLatency();

    check (fx.latencySamples() == kLatencySamples,
           "PluginFx reports 240 samples for a 5 ms effect");
    check (chain.latencySamples() == kLatencySamples,
           "the FX chain total includes the plugin's 240 samples");

    return 0;
}

}
