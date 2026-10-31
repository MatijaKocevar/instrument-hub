// ihub-fx-chain-test: acceptance check for T031.
//
// 1. An empty FxChain is a bit-exact passthrough. Verified directly on an
//    audio buffer and through the full Slot path (source -> chain -> output).
// 2. Two TestPlugin instances loaded from the in-repo VST3 are inserted as
//    effects. One is a pure gain, the other adds a sine after its gain, so the
//    result depends on order: gain-then-sine differs from sine-then-gain.
// 3. Slot::latencySamples() sums the instrument/source latency and the chain
//    latency (a TestPlugin set to 5 ms contributes 240 samples at 48 kHz).
//
// The bundle path is injected by CMake (IHUB_TESTPLUGIN_VST3); an explicit
// path may also be passed as the first command-line argument.

#include "audio/Bus.h"
#include "audio/FxChain.h"
#include "audio/Slot.h"
#include "plugins/PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr int kFadeBlocks = 8; // 1024 frames, well past the slot's 3 ms ramp

constexpr float kGainA = 0.5f;
constexpr float kSineLevelB = 0.1f;
constexpr float kGainB = 0.25f;
constexpr int kLatencySamples = 240; // 5 ms at 48 kHz

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool buffersBitEqual (const juce::AudioBuffer<float>& a,
                      const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels()
        || a.getNumSamples() != b.getNumSamples())
        return false;

    const auto bytes = static_cast<std::size_t> (a.getNumSamples())
                       * sizeof (float);

    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        if (std::memcmp (a.getReadPointer (channel),
                         b.getReadPointer (channel),
                         bytes) != 0)
            return false;

    return true;
}

/** A deterministic source whose pattern is identical on every block. */
class PatternSource final : public AudioEngine::SlotSource
{
public:
    void prepare (double, int maximumBlockSize) override
    {
        pattern.resize (static_cast<std::size_t> (juce::jmax (1, maximumBlockSize)));

        for (std::size_t i = 0; i < pattern.size(); ++i)
        {
            const auto value = static_cast<int> ((i * 37u) % 200u) - 100;
            pattern[i] = static_cast<float> (value) / 250.0f;
        }
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int samples = juce::jmin (buffer.getNumSamples(),
                                        static_cast<int> (pattern.size()));

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.getWritePointer (channel)[i] = pattern[static_cast<std::size_t> (i)];
    }

    int latencySamples() const noexcept override { return 24; }

    const std::vector<float>& getPattern() const noexcept { return pattern; }

private:
    std::vector<float> pattern;
};

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types)
{
    const auto path = bundle.getFullPathName();

    for (auto* format : manager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    for (auto* type : types)
        if (type->name.contains ("TestPlugin"))
            return type;

    return nullptr;
}

void fillPattern (juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = static_cast<int> ((i * 13 + channel * 29) % 100) - 50;
            buffer.setSample (channel, i, static_cast<float> (value) / 100.0f);
        }
}

//==============================================================================
void testEmptyChainIsBitExact()
{
    std::cout << "\n-- empty chain is a bit-exact passthrough --" << std::endl;

    AudioEngine::FxChain chain;
    chain.prepare (kSampleRate, kBlockSize, kNumChannels);

    check (chain.isEmpty(), "a fresh chain is empty");
    check (chain.size() == 0, "size() is zero");
    check (chain.latencySamples() == 0, "an empty chain reports zero latency");

    juce::AudioBuffer<float> block (kNumChannels, kBlockSize);
    fillPattern (block);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (block);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

    const int midiEventsBefore = midi.getNumEvents();

    chain.process (block, midi);

    check (buffersBitEqual (block, reference),
           "FxChain::process leaves the buffer bit-identical when empty");
    check (midi.getNumEvents() == midiEventsBefore,
           "FxChain::process leaves the MIDI buffer untouched when empty");
}

void testEmptyChainThroughSlot()
{
    std::cout << "\n-- empty chain through the Slot path --" << std::endl;

    PatternSource source;
    AudioEngine::Slot slot (0);

    check (slot.getFxChain().isEmpty(), "a new slot starts with an empty FX chain");

    slot.setSource (&source);
    slot.prepare (kSampleRate, kBlockSize, kNumChannels);
    slot.activate();

    juce::MidiBuffer midi;
    bool processedEveryBlock = true;

    for (int block = 0; block < kFadeBlocks; ++block)
        if (! slot.process (kBlockSize, midi))
            processedEveryBlock = false;

    check (processedEveryBlock, "the slot processes every block during its fade-in");
    check (slot.getState() == AudioEngine::SlotState::active,
           "the slot is active after the fade ramp");

    const auto& output = slot.getOutputBuffer();
    const auto& pattern = source.getPattern();

    bool identical = output.getNumChannels() == kNumChannels
                     && output.getNumSamples() == kBlockSize;

    if (identical)
        for (int channel = 0; channel < kNumChannels; ++channel)
            if (std::memcmp (output.getReadPointer (channel),
                             pattern.data(),
                             static_cast<std::size_t> (kBlockSize) * sizeof (float)) != 0)
                identical = false;

    check (identical,
           "slot output is bit-identical to the source with an empty chain");
    check (slot.latencySamples() == source.latencySamples(),
           "an empty chain adds no latency to the slot total");
}

//==============================================================================
int testChainOrder (const juce::File& bundle)
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

    // A is a pure 0.5 gain. B applies 0.25 gain and then adds a 0.1 sine.
    check (a.setParameterValue ("Gain", kGainA), "A gain is set");
    check (a.setParameterValue ("Sine Level", 0.0f), "A sine is disabled");
    check (b.setParameterValue ("Gain", kGainB), "B gain is set");
    check (b.setParameterValue ("Sine Level", kSineLevelB), "B sine level is set");

    juce::AudioBuffer<float> input (kNumChannels, kBlockSize);
    fillPattern (input);

    // --- Forward: A then B --------------------------------------------------
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

    // --- Reverse: B then A --------------------------------------------------
    AudioEngine::FxChain reverse;
    check (reverse.add (b.getPlugin()) == 0, "B is appended at index 0");
    check (reverse.add (a.getPlugin()) == 1, "A is appended at index 1");

    // JUCE skips a redundant prepareToPlay() on an active VST3, so release
    // first: that makes the plugin reset its sine phase and delay line.
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

    // --- Latency flows into the slot total ---------------------------------
    // 5 ms on a 0..20 ms parameter => 0.25 normalised. At 48 kHz that is
    // exactly 240 samples.
    check (a.setParameterValue ("Latency", 5.0f / 20.0f),
           "A latency is set to 5 ms");

    AudioEngine::FxChain latencyChain;
    check (latencyChain.add (a.getPlugin()) == 0, "A is added to the latency chain");

    // Same JUCE short-circuit: release the active instance so prepareToPlay()
    // runs again and caches the new latency parameter.
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
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;

    testEmptyChainIsBitExact();
    testEmptyChainThroughSlot();

    if (bundle.isDirectory())
        testChainOrder (bundle);
    else
        check (false, "TestPlugin.vst3 bundle exists");

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
