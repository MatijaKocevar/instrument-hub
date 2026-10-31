// ihub-plugin-instance-test: acceptance check for T022.
//
// Proves that PluginInstrument hosts a real VST3 end to end:
//
//   1. TestPlugin.vst3 is instantiated through juce::AudioPluginFormatManager
//      and wrapped in PluginInstrument
//   2. a non-default parameter (the DSP-irrelevant "State Marker") is set,
//      the plugin state is saved to a MemoryBlock, a *second* instance is
//      created and the blob is restored into it, and the parameter matches
//   3. prepare() caches the plugin's artificial latency (10 ms = 480 samples)
//      and process() renders the sine generator
//   4. noteOn()/noteOff() reach the plugin through the lock-free queue
//   5. unload() leaves the wrapper safe to call process() on
//
// The bundle path is injected by CMake (IHUB_TESTPLUGIN_VST3); an explicit
// path may also be passed as the first command-line argument.

#include "plugins/PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{
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

constexpr float stateMarkerNormalised = 0.875f;
constexpr float frequencyNormalised = (1000.0f - 20.0f) / (20000.0f - 20.0f);
} // namespace

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

    juce::AudioPluginFormatManager formatManager;
    juce::addHeadlessDefaultFormatsToManager (formatManager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* type = findTestPlugin (formatManager, bundle, types);

    check (type != nullptr, "TestPlugin type is enumerated by the format manager");

    if (type == nullptr)
        return 1;

    const juce::PluginDescription description = *type;

    // --- 1. Load and set a non-default parameter ---------------------------
    ihub::plugins::PluginInstrument instrument;
    juce::String error;

    check (instrument.load (formatManager, description, 48000.0, 512, error),
           "PluginInstrument::load succeeded" + (error.isEmpty() ? std::string() : " (" + error.toStdString() + ")"));

    if (! instrument.isLoaded())
        return 1;

    std::cout << "loaded: " << instrument.getName() << std::endl;

    check (instrument.getName().contains ("TestPlugin"), "the instance reports its name");
    check (instrument.getNumParameters() >= 5, "the instance exposes all five parameters");
    check (instrument.configureBuses (2), "the instance accepts a stereo main bus");

    ihub::Instrument* asInstrument = &instrument;
    AudioEngine::SlotSource* asSlotSource = &instrument;

    check (asInstrument->kind() == ihub::Instrument::Kind::plugin,
           "kind() reports Instrument::Kind::plugin");
    check (asSlotSource != nullptr, "the wrapper is usable as an AudioEngine::SlotSource");

    check (instrument.setParameterValue ("State Marker", stateMarkerNormalised),
           "State Marker parameter exists and was set");
    check (instrument.setParameterValue ("Frequency", frequencyNormalised),
           "Frequency parameter was set");
    check (instrument.setParameterValue ("Sine Level", 1.0f),
           "Sine Level parameter was set");
    check (instrument.setParameterValue ("Latency", 0.5f),
           "Latency parameter was set to 10 ms");

    check (nearlyEqual (instrument.getParameterValue ("State Marker"), stateMarkerNormalised),
           "State Marker reads back the value that was set");

    std::cout << "state marker: " << instrument.getParameterValue ("State Marker")
              << " frequency: " << instrument.getParameterText ("Frequency") << std::endl;

    // --- 2. Save state and reload into a new instance ----------------------
    juce::MemoryBlock state;
    instrument.getStateInformation (state);

    std::cout << "state blob: " << state.getSize() << " bytes" << std::endl;

    check (state.getSize() > 0, "getStateInformation produced a non-empty blob");

    ihub::plugins::PluginInstrument reloaded;
    juce::String reloadError;

    check (reloaded.load (formatManager, description, 48000.0, 512, reloadError),
           "a second instance of the same type loads");

    if (! reloaded.isLoaded())
        return 1;

    const float markerBeforeRestore = reloaded.getParameterValue ("State Marker");

    std::cout << "state marker before restore: " << markerBeforeRestore << std::endl;

    check (! nearlyEqual (markerBeforeRestore, stateMarkerNormalised),
           "a fresh instance starts on the default State Marker, not the saved one");

    check (reloaded.setStateInformation (state.getData(), static_cast<int> (state.getSize())),
           "setStateInformation accepted the blob");

    const float markerAfterRestore = reloaded.getParameterValue ("State Marker");
    const float frequencyAfterRestore = reloaded.getParameterValue ("Frequency");

    std::cout << "state marker after restore: " << markerAfterRestore
              << " frequency (normalised): " << frequencyAfterRestore << std::endl;

    check (nearlyEqual (markerAfterRestore, stateMarkerNormalised),
           "State Marker matches after the state round-trip");

    check (nearlyEqual (frequencyAfterRestore, frequencyNormalised),
           "Frequency matches after the state round-trip");

    // --- 3. Prepare, latency and the sine generator ------------------------
    instrument.prepare (48000.0, 512);

    const int expectedLatency = static_cast<int> (std::lround (10.0 * 0.001 * 48000.0));

    std::cout << "latency: cached " << asSlotSource->latencySamples()
              << ", expected " << expectedLatency << std::endl;

    check (asSlotSource->latencySamples() == expectedLatency,
           "10 ms at 48 kHz is cached as 480 samples after prepare()");
    check (asInstrument->latencySamples() == expectedLatency,
           "the Instrument interface reports the same latency");

    // Remove the artificial delay so the whole block carries the sine.
    instrument.setParameterValue ("Latency", 0.0f);

    juce::AudioBuffer<float> block (2, 512);
    juce::MidiBuffer midi;
    block.clear();
    instrument.process (block, midi);

    const auto rms = block.getRMSLevel (0, 0, block.getNumSamples());
    std::cout << "sine RMS after process(): " << rms << std::endl;

    check (rms > 0.3f, "process() renders the TestPlugin sine generator");

    // --- 4. MIDI in through the lock-free queue ----------------------------
    check (instrument.getPendingMidiEventCount() == 0, "the note queue starts empty");

    instrument.noteOn (1, 60, 100.0f / 127.0f);
    instrument.noteOff (1, 60);

    check (instrument.getPendingMidiEventCount() == 2,
           "noteOn/noteOff are queued for the next block");

    block.clear();
    midi.clear();
    instrument.process (block, midi);

    std::cout << "midi: delivered " << instrument.getDeliveredMidiEventCount()
              << ", dropped " << instrument.getDroppedMidiEventCount()
              << ", pending " << instrument.getPendingMidiEventCount() << std::endl;

    check (instrument.getPendingMidiEventCount() == 0, "process() drained the note queue");
    check (instrument.getDeliveredMidiEventCount() == 2,
           "both note events were handed to the plugin");
    check (instrument.getDroppedMidiEventCount() == 0, "no MIDI events were dropped");

    // Overflow must drop rather than block or allocate.
    for (int i = 0; i < ihub::plugins::PluginInstrument::maxPendingMidiEvents * 2; ++i)
        instrument.noteOn (1, 60 + (i % 12), 1.0f);

    check (instrument.getDroppedMidiEventCount() > 0,
           "a full note queue drops events instead of blocking");

    midi.clear();
    block.clear();
    instrument.process (block, midi);
    check (instrument.getPendingMidiEventCount() == 0, "the queue empties again after a full drain");

    // --- 5. Unload ---------------------------------------------------------
    instrument.unload();

    check (! instrument.isLoaded(), "unload() reports the instance as unloaded");

    for (int channel = 0; channel < block.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill (block.getWritePointer (channel), 1.0f,
                                           block.getNumSamples());

    midi.clear();
    instrument.process (block, midi);

    check (block.getMagnitude (0, 0, block.getNumSamples()) <= 1.0e-9f,
           "process() after unload() is silent instead of crashing");

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
