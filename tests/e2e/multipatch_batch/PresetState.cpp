#include "multipatch_batch/PresetState.h"
#include "multipatch_batch/Constants.h"
#include "multipatch_batch/Patches.h"

#include "plugins/PluginInstrument.h"

#include <cstdint>
#include <cstdio>
#include <utility>

namespace MultiPatch
{
namespace
{
float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}
}

bool createPresetStates (const juce::File& bundle,
                         int firstPatch,
                         int count,
                         PresetStates& states,
                         juce::String& error)
{
    error.clear();
    states.clear();

    juce::AudioPluginFormatManager formats;
    juce::addHeadlessDefaultFormatsToManager (formats);

    ihub::plugins::PluginInstrument generator;

    if (! generator.loadFromFile (formats, bundle, kSampleRate, kBlockSize, error))
        return false;

    states.reserve (static_cast<std::size_t> (count));

    for (int offset = 0; offset < count; ++offset)
    {
        const auto preset = presetForPatch (firstPatch + offset);
        const bool configured =
            generator.setParameterValue ("Sine Level", 1.0f)
            && generator.setParameterValue ("Gain", 0.0f)
            && generator.setParameterValue ("Latency", 0.0f)
            && generator.setParameterValue ("MIDI Gate", 0.0f)
            && generator.setParameterValue ("Frequency", normaliseFrequency (preset.frequency))
            && generator.setParameterValue ("State Marker", preset.marker);

        if (! configured)
        {
            error = "patch " + juce::String (preset.index)
                  + " did not accept its preset parameters";
            return false;
        }

        auto state = generator.getStateInformation();

        if (state.getSize() == 0)
        {
            error = "patch " + juce::String (preset.index) + " produced an empty state";
            return false;
        }

        states.push_back (std::move (state));
    }

    generator.unload();
    return true;
}

std::string hashOf (const juce::MemoryBlock& data)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto* bytes = static_cast<const std::uint8_t*> (data.getData());

    for (std::size_t index = 0; index < data.getSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }

    char text[32] {};
    std::snprintf (text, sizeof (text), "%016llx",
                   static_cast<unsigned long long> (hash));
    return text;
}

}
