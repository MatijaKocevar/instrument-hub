#include "ChainOrder.h"
#include "Fixtures.h"

#include "audio/FxChain.h"
#include "plugins/PluginInstrument.h"

#include <cmath>
#include <iostream>

namespace FxChainTest
{

int runChainOrder (const juce::File& bundle)
{
    std::cout << "\n-- two TestPlugin effects apply in chain order --" << std::endl;

    juce::AudioPluginFormatManager manager;
    juce::addHeadlessDefaultFormatsToManager (manager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* type = findTestPlugin (manager, bundle, types);

    check (type != nullptr, "TestPlugin is enumerated by the format manager");

    if (type == nullptr)
        return 1;

    const juce::PluginDescription description = *type;

    ihub::plugins::PluginInstrument a;
    ihub::plugins::PluginInstrument b;
    juce::String error;

    check (a.load (manager, description, kSampleRate, kBlockSize, error),
           "first TestPlugin instance loads");
    check (b.load (manager, description, kSampleRate, kBlockSize, error),
           "second TestPlugin instance loads");

    if (! a.isLoaded() || ! b.isLoaded())
        return 1;

    check (a.setParameterValue ("Gain", kGainA), "A gain is set");
    check (a.setParameterValue ("Sine Level", 0.0f), "A sine is disabled");
    check (b.setParameterValue ("Gain", kGainB), "B gain is set");
    check (b.setParameterValue ("Sine Level", kSineLevelB), "B sine level is set");

    juce::AudioBuffer<float> input (kNumChannels, kBlockSize);
    fillPattern (input);

    AudioEngine::FxChain forward;
    check (forward.add (a.getPlugin()) == 0, "A is appended at index 0");
    check (forward.add (b.getPlugin()) == 1, "B is appended at index 1");
    check (forward.indexOf (b.getPlugin()) == 1, "indexOf() follows list order");

    forward.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (a.getPlugin()->getTotalNumInputChannels() == kNumChannels,
           "preparing a chain enables the plugin's main input bus");
    check (forward.latencySamples() == 0,
           "two zero-latency effects report zero chain latency");

    juce::AudioBuffer<float> forwardOutput;
    forwardOutput.makeCopyOf (input);

    juce::MidiBuffer midi;
    forward.process (forwardOutput, midi);

    double forwardError = 0.0;
    double phase = 0.0;
    const double phaseIncrement = juce::MathConstants<double>::twoPi * 440.0 / kSampleRate;

    for (int i = 0; i < kBlockSize; ++i)
    {
        const float sine = static_cast<float> (std::sin (phase)) * kSineLevelB;
        phase += phaseIncrement;

        for (int channel = 0; channel < kNumChannels; ++channel)
        {
            const float x = input.getSample (channel, i);
            const float expected = (x * kGainA) * kGainB + sine;
            forwardError = juce::jmax (
                forwardError,
                std::abs (static_cast<double> (forwardOutput.getSample (channel, i) - expected)));
        }
    }

    check (forwardError < 1.0e-6,
           "A then B matches gain 0.5 -> (gain 0.25 + sine 0.1)");

    AudioEngine::FxChain reverse;
    check (reverse.add (b.getPlugin()) == 0, "B is appended at index 0");
    check (reverse.add (a.getPlugin()) == 1, "A is appended at index 1");

    forward.releaseResources();

    reverse.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::AudioBuffer<float> reverseOutput;
    reverseOutput.makeCopyOf (input);
    reverse.process (reverseOutput, midi);

    double reverseError = 0.0;
    double reverseDifference = 0.0;
    phase = 0.0;

    for (int i = 0; i < kBlockSize; ++i)
    {
        const float sine = static_cast<float> (std::sin (phase)) * kSineLevelB;
        phase += phaseIncrement;

        for (int channel = 0; channel < kNumChannels; ++channel)
        {
            const float x = input.getSample (channel, i);
            const float expected = (x * kGainB + sine) * kGainA;
            const float forwardExpected = (x * kGainA) * kGainB + sine;

            reverseError = juce::jmax (
                reverseError,
                std::abs (static_cast<double> (reverseOutput.getSample (channel, i) - expected)));

            reverseDifference = juce::jmax (
                reverseDifference,
                std::abs (static_cast<double> (expected - forwardExpected)));
        }
    }

    check (reverseError < 1.0e-6,
           "B then A matches (gain 0.25 + sine 0.1) -> gain 0.5");
    check (reverseDifference > 0.04,
           "the two orders produce measurably different output");

    check (a.setParameterValue ("Latency", 5.0f / 20.0f),
           "A latency is set to 5 ms");

    AudioEngine::FxChain latencyChain;
    check (latencyChain.add (a.getPlugin()) == 0, "A is added to the latency chain");

    a.getPlugin()->releaseResources();
    latencyChain.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (latencyChain.latencySamples() == kLatencySamples,
           "a 5 ms effect reports 240 samples of chain latency");

    PatternSource source;
    AudioEngine::Slot slot (0);
    slot.setSource (&source);
    slot.prepare (kSampleRate, kBlockSize, kNumChannels);

    const int latencyBefore = slot.latencySamples();
    const double latencyMsBefore = slot.latencyMilliseconds();

    check (slot.getFxChain().add (a.getPlugin()) == 0,
           "the effect is added to the slot's chain");

    const int latencyAfter = slot.latencySamples();
    const double latencyMsAfter = slot.latencyMilliseconds();

    check (latencyAfter == source.latencySamples() + kLatencySamples,
           "Slot::latencySamples() sums source and chain latency");
    check (latencyAfter - latencyBefore == kLatencySamples,
           "inserting a 5 ms plugin adds exactly 240 samples at 48 kHz");
    check (std::abs ((latencyMsAfter - latencyMsBefore) - 5.0) < 1.0e-9,
           "Slot::latencyMilliseconds() increases by exactly 5 ms");

    return 0;
}

} // namespace FxChainTest
