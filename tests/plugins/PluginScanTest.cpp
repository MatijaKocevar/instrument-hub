#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <cmath>
#include <iostream>
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

juce::String normalised (const juce::String& path)
{
    auto result = juce::File (path).getFullPathName();

    while (result.endsWithChar (juce::File::getSeparatorChar())
           || result.endsWithChar ('/'))
        result = result.dropLastCharacters (1);

    return result;
}

bool containsBundle (const juce::StringArray& paths, const juce::File& bundle)
{
    const auto wanted = normalised (bundle.getFullPathName());

    for (const auto& path : paths)
        if (normalised (path) == wanted)
            return true;

    return false;
}

juce::AudioProcessorParameter* findParameter (juce::AudioProcessor& processor,
                                              const juce::String& parameterID,
                                              const juce::String& parameterName)
{
    for (auto* parameter : processor.getParameters())
    {
        if (parameter->getName (64) == parameterName)
            return parameter;

        if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            if (hosted->getParameterID() == parameterID)
                return parameter;
    }

    return nullptr;
}

void setNormalisedParameter (juce::AudioProcessor& processor,
                             const juce::String& parameterID,
                             const juce::String& parameterName,
                             float normalisedValue)
{
    if (auto* parameter = findParameter (processor, parameterID, parameterName))
        parameter->setValueNotifyingHost (normalisedValue);
    else
        check (false, "parameter '" + parameterName.toStdString() + "' exists");
}
}

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
    juce::addHeadlessDefaultFormatsToManager (manager);

    std::cout << "formats:";

    for (auto* format : manager.getFormats())
        std::cout << " " << format->getName();

    std::cout << std::endl;

    juce::OwnedArray<juce::PluginDescription> types;
    juce::AudioPluginFormat* vst3 = nullptr;

    for (auto* format : manager.getFormats())
    {
        if (format->getName() == juce::String ("VST3"))
            vst3 = format;

        if (format->fileMightContainThisPluginType (bundle.getFullPathName()))
            format->findAllTypesForFile (types, bundle.getFullPathName());
    }

    check (vst3 != nullptr, "AudioPluginFormatManager has a VST3 format");
    check (! types.isEmpty(), "findAllTypesForFile found at least one plugin in the bundle");

    juce::PluginDescription* testPlugin = nullptr;

    for (auto* type : types)
        if (type->name.contains ("TestPlugin"))
            testPlugin = type;

    check (testPlugin != nullptr, "the enumerated type is named TestPlugin");

    if (testPlugin == nullptr)
        return 1;

    std::cout << "found: " << testPlugin->name << " (" << testPlugin->pluginFormatName
              << ", " << testPlugin->manufacturerName << ")" << std::endl;

    if (vst3 != nullptr)
    {
        const juce::FileSearchPath searchPath (bundle.getParentDirectory().getFullPathName());
        const auto found = vst3->searchPathsForPlugins (searchPath, true, true);

        check (containsBundle (found, bundle),
               "searchPathsForPlugins over the artefacts directory finds the bundle");

        for (const auto& path : found)
            std::cout << "scan: " << path << std::endl;
    }

    juce::String error;
    auto instance = manager.createPluginInstance (*testPlugin, 48000.0, 512, error);

    check (instance != nullptr, "createPluginInstance succeeded");

    if (instance == nullptr)
    {
        std::cout << "create error: " << error << std::endl;
        return 1;
    }

    check (instance->getName().contains ("TestPlugin"), "instance reports its name");
    check (instance->getParameters().size() >= 5, "instance exposes all five parameters");

    for (auto* parameter : instance->getParameters())
    {
        juce::String id;

        if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            id = hosted->getParameterID();

        std::cout << "param: id='" << id << "' name='" << parameter->getName (64)
                  << "'" << std::endl;
    }

    setNormalisedParameter (*instance, "gain", "Gain", 0.0f);
    setNormalisedParameter (*instance, "frequency", "Frequency", (1000.0f - 20.0f) / (20000.0f - 20.0f));
    setNormalisedParameter (*instance, "latencyMs", "Latency", 0.5f);
    setNormalisedParameter (*instance, "sineLevel", "Sine Level", 1.0f);
    setNormalisedParameter (*instance, "stateMarker", "State Marker", 0.875f);

    for (auto* parameter : instance->getParameters())
        std::cout << "value: " << parameter->getName (64) << " = "
                  << parameter->getValue() << std::endl;

    instance->prepareToPlay (48000.0, 512);

    const int expectedLatency = static_cast<int> (std::lround (10.0 * 0.001 * 48000.0));
    const int reportedLatency = instance->getLatencySamples();

    std::cout << "latency: reported " << reportedLatency
              << " samples, expected " << expectedLatency << std::endl;

    check (reportedLatency == expectedLatency,
           "10 ms at 48 kHz is reported as 480 samples");

    juce::AudioBuffer<float> block (2, 512);
    juce::MidiBuffer midi;
    block.clear();
    instance->processBlock (block, midi);

    const auto rms = block.getRMSLevel (0, 0, block.getNumSamples());
    const auto magnitude = block.getMagnitude (0, 0, block.getNumSamples());

    int firstNonZero = -1;

    for (int sample = 0; sample < block.getNumSamples(); ++sample)
        if (std::abs (block.getSample (0, sample)) > 1.0e-6f)
        {
            firstNonZero = sample;
            break;
        }

    std::cout << "sine RMS: " << rms << " magnitude: " << magnitude
              << " first non-zero sample: " << firstNonZero << std::endl;

    check (rms > 0.05f, "the sine generator produces audio");

    instance->releaseResources();
    instance.reset();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
