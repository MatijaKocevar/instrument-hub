#include "analog_lab/Bundle.h"

#include <cstdlib>

namespace AnalogLab
{

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
