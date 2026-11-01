#include "Check.h"
#include "analog_lab/SessionRig.h"
#include "analog_lab/StateFile.h"

#include "plugins/PluginInstrument.h"

#include <cstdint>
#include <cstdio>

namespace AnalogLab
{
using E2E::check;


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

bool stabiliseState (ihub::plugins::PluginInstrument& instrument,
                     juce::MemoryBlock& result)
{
    juce::MemoryBlock candidate = instrument.getStateInformation();

    for (int attempt = 0; attempt < 6; ++attempt)
    {
        if (candidate.getSize() == 0)
            return false;

        (void) instrument.setStateInformation (candidate);
        pumpFor (250);

        auto next = instrument.getStateInformation();

        if (next.getSize() == 0)
            return false;

        if (next == candidate)
        {
            result = candidate;
            return true;
        }

        candidate = std::move (next);
    }

    result = candidate;
    return result.getSize() > 0;
}

bool verifySlotState (ihub::Session& session,
                      int slot,
                      const std::string& expectedHash,
                      const std::string& phase)
{
    auto* instrument = session.getPluginInstrument (slot);

    if (instrument == nullptr)
    {
        check (false, phase + ": slot " + std::to_string (slot)
                          + " has no live Analog Lab V instance");
        return false;
    }

    const auto liveHash = hashOf (instrument->getStateInformation());
    const bool matches = liveHash == expectedHash;

    check (matches,
           phase + ": slot " + std::to_string (slot) + " live state hash "
               + liveHash + " matches " + expectedHash);

    return matches;
}

bool writeHashes (const juce::File& file,
                  const std::string& hashA,
                  const std::string& hashB)
{
    return file.replaceWithText (juce::String (hashA) + "\n" + juce::String (hashB)
                                 + "\n");
}

bool readHashes (const juce::File& file, std::string& hashA, std::string& hashB)
{
    if (! file.existsAsFile())
        return false;

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    lines.removeEmptyStrings();

    if (lines.size() < 2)
        return false;

    hashA = lines[0].toStdString();
    hashB = lines[1].toStdString();
    return hashA.size() == 16 && hashB.size() == 16;
}

}
