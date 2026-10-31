// ihub-plugin-fx-test: acceptance check for T063.
//
// Proves that a hosted VST3 can be used as an FX-chain element:
//
//   1. TestPlugin is loaded through PluginFx in FX mode, with its normally
//      disabled main input bus enabled next to the output bus.
//   2. The plugin is inserted into an AudioEngine::FxChain and its Gain
//      parameter is applied to the block (0.5 -> half amplitude).
//   3. Setting the plugin's Latency parameter to 5 ms at 48 kHz makes
//      PluginFx report 240 samples and the chain total include exactly 240.
//   4. Plugin state round-trips into a second instance through
//      getStateInformation()/setStateInformation().
//   5. The native editor opens in an app-owned window, closes explicitly, and
//      is closed by unload() before the processor is destroyed.
//
// The bundle path is injected by CMake (IHUB_TESTPLUGIN_VST3); an explicit
// path may also be passed as the first command-line argument. Run under
// DISPLAY=:0 because the editor is a real window.

#include "audio/FxChain.h"
#include "instruments/PluginFx.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <iostream>
#include <string>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;

constexpr float kGain = 0.5f;
constexpr float kLatencyMs = 5.0f;
constexpr int kLatencySamples = 240; // 5 ms at 48 kHz

constexpr float kStateMarker = 0.875f;
constexpr float kStateGain = 0.25f;
constexpr float kStateFrequency = (1000.0f - 20.0f) / (20000.0f - 20.0f);

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool nearlyEqual (float a, float b, float tolerance = 1.0e-3f)
{
    return std::abs (a - b) <= tolerance;
}

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

void pumpMessages (int milliseconds)
{
    auto* messageManager = juce::MessageManager::getInstance();

    if (messageManager == nullptr)
        return;

    const auto deadline = juce::Time::getMillisecondCounter()
                          + static_cast<juce::uint32> (juce::jmax (0, milliseconds));

    while (juce::Time::getMillisecondCounter() < deadline)
        messageManager->runDispatchLoopUntil (10);
}

//==============================================================================
int testGainAndLatency (juce::AudioPluginFormatManager& manager,
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

    // --- Insert into the chain and check the audio -------------------------
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

    // --- Latency flows into the chain total --------------------------------
    check (fx.setParameterValue ("Latency", kLatencyMs / 20.0f),
           "Latency is set to 5 ms");

    // TestPlugin adopts a changed latency parameter in processBlock(); run one
    // silent block so it reaches setLatencySamples(), then ask the host-side
    // caches to refresh, exactly as a control-thread update would.
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

//==============================================================================
void testStateRoundTrip (juce::AudioPluginFormatManager& manager,
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

//==============================================================================
void testEditor (juce::AudioPluginFormatManager& manager,
                 const juce::PluginDescription& description)
{
    std::cout << "\n-- the native editor opens and closes with the wrapper --" << std::endl;

    ihub::plugins::PluginFx fx;
    juce::String error;

    check (fx.load (manager, description, kSampleRate, kBlockSize, error),
           "the editor host loads TestPlugin");

    if (! fx.isLoaded())
        return;

    check (fx.openEditor ("TestPlugin FX"), "openEditor creates the native editor");
    check (fx.isEditorOpen(), "the editor reports open");
    check (fx.getEditor() != nullptr, "the editor component exists");
    check (fx.getEditorWindow() != nullptr, "the editor has an app-owned window");

    pumpMessages (150);

    check (fx.isEditorOpen(), "the editor is still open after message-loop time");

    fx.closeEditor();
    check (! fx.isEditorOpen(), "closeEditor destroys the editor window");
    check (fx.getEditor() == nullptr, "no editor remains after close");

    check (fx.openEditor ("TestPlugin FX"), "the editor can be reopened");

    fx.unload();

    check (! fx.isEditorOpen(), "unload() closes the editor before destroying the plugin");
    check (! fx.isLoaded(), "unload() leaves the wrapper empty");
    check (fx.getProcessor() == nullptr, "no processor remains after unload");
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;

    check (bundle.isDirectory(), "TestPlugin.vst3 bundle exists");

    if (! bundle.isDirectory())
        return 1;

    juce::AudioPluginFormatManager manager;
    juce::addDefaultFormatsToManager (manager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* type = findTestPlugin (manager, bundle, types);

    check (type != nullptr, "TestPlugin is enumerated by the format manager");

    if (type == nullptr)
        return 1;

    const juce::PluginDescription description = *type;

    testGainAndLatency (manager, description);
    testStateRoundTrip (manager, description);
    testEditor (manager, description);

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
