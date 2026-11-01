#include "plugin_fx/Fixtures.h"
#include "plugin_fx/Editor.h"
#include "plugin_fx/GainLatency.h"
#include "plugin_fx/State.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;

    PluginFxTest::check (bundle.isDirectory(), "TestPlugin.vst3 bundle exists");

    if (! bundle.isDirectory())
        return 1;

    juce::AudioPluginFormatManager manager;
    juce::addDefaultFormatsToManager (manager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* type = PluginFxTest::findTestPlugin (manager, bundle, types);

    PluginFxTest::check (type != nullptr, "TestPlugin is enumerated by the format manager");

    if (type == nullptr)
        return 1;

    const juce::PluginDescription description = *type;

    PluginFxTest::runGainAndLatency (manager, description);
    PluginFxTest::runStateRoundTrip (manager, description);
    PluginFxTest::runEditor (manager, description);

    std::cout << (PluginFxTest::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << PluginFxTest::failures << " failed check(s)" << std::endl;

    return PluginFxTest::failures == 0 ? 0 : 1;
}
