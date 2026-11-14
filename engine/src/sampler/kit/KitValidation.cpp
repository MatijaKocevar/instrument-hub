#include "../Kit.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace ihub::sampler
{

    namespace
    {

        bool isFiniteInRange (double value, double minimum, double maximum) noexcept
        {
            return std::isfinite (value) && value >= minimum && value <= maximum;
        }

    }

    bool Kit::validate (juce::String& error) const
    {
        const auto fail = [&error] (const juce::String& message)
        {
            error = message;
            return false;
        };

        if (schemaVersion != currentVersion)
            return fail ("unsupported kit schema version " + juce::String (schemaVersion) +
                         " (expected " + juce::String (currentVersion) + ")");

        if (name.empty ())
            return fail ("kit name must not be empty");

        if (name.size () > maximumKitNameBytes)
            return fail ("kit name is longer than " +
                         juce::String (static_cast<juce::int64> (maximumKitNameBytes)) + " bytes");

        if (pads.empty ())
            return fail ("a kit must have at least one pad");

        if (pads.size () > (std::size_t)maximumKitPads)
            return fail ("a kit can have at most " + juce::String (maximumKitPads) + " pads");

        std::set<int> usedNotes;
        int sampleReferences = 0;

        for (const auto& pad : pads)
        {
            const auto padName = pad.name.empty () ? juce::String ("pad ") + juce::String (pad.note)
                                                   : juce::String (pad.name);

            if (pad.note < minimumMidiNote || pad.note > maximumMidiNote)
                return fail (padName + ": note must be between 0 and 127");

            if (!usedNotes.insert (pad.note).second)
                return fail ("duplicate pad note: " + juce::String (pad.note));

            if (pad.name.size () > maximumKitPadNameBytes)
                return fail ("pad " + juce::String (pad.note) + ": name is longer than " +
                             juce::String (static_cast<juce::int64> (maximumKitPadNameBytes)) +
                             " bytes");

            if (!isFiniteInRange (pad.gain, 0.0, 4.0))
                return fail (padName + ": gain must be between 0 and 4");

            if (!isFiniteInRange (pad.pan, -1.0, 1.0))
                return fail (padName + ": pan must be between -1 and 1");

            if (!isFiniteInRange (pad.tuneSemitones, -maximumTuneSemitones, maximumTuneSemitones))
                return fail (padName + ": tune must be between -48 and 48 semitones");

            if (pad.layers.empty ())
                return fail (padName + ": needs at least one velocity layer");

            if (pad.layers.size () > (std::size_t)maximumLayersPerPad)
                return fail (padName + ": has more than " + juce::String (maximumLayersPerPad) +
                             " velocity layers");

            std::vector<std::pair<int, int>> ranges;
            ranges.reserve (pad.layers.size ());

            for (const auto& layer : pad.layers)
            {
                if (layer.minVelocity < minimumMidiVelocity ||
                    layer.minVelocity > maximumMidiVelocity ||
                    layer.maxVelocity < minimumMidiVelocity ||
                    layer.maxVelocity > maximumMidiVelocity)
                {
                    return fail (padName + ": velocity range must be within 1..127");
                }

                if (layer.minVelocity > layer.maxVelocity)
                    return fail (padName + ": layer minVelocity is greater than maxVelocity");

                if (layer.roundRobin.empty ())
                    return fail (padName + ": layer " + juce::String (layer.minVelocity) + ".." +
                                 juce::String (layer.maxVelocity) + " has no round-robin samples");

                if (layer.roundRobin.size () > (std::size_t)maximumRoundRobinPerLayer)
                    return fail (padName + ": a layer has more than " +
                                 juce::String (maximumRoundRobinPerLayer) + " variants");

                for (const auto& sample : layer.roundRobin)
                {
                    if (!isSafeSamplePath (sample.path))
                        return fail (padName + ": unsafe or empty sample path \"" +
                                     juce::String (sample.path) + "\"");

                    if (!isFiniteInRange (sample.gain, 0.0, 4.0))
                        return fail (padName + ": sample gain must be between 0 and 4");

                    if (!isFiniteInRange (sample.pan, -1.0, 1.0))
                        return fail (padName + ": sample pan must be between -1 and 1");

                    if (!isFiniteInRange (
                            sample.tuneSemitones, -maximumTuneSemitones, maximumTuneSemitones))
                        return fail (padName +
                                     ": sample tune must be between -48 and 48 semitones");

                    ++sampleReferences;

                    if (sampleReferences > maximumKitSampleReferences)
                        return fail ("a kit can reference at most " +
                                     juce::String (maximumKitSampleReferences) + " samples");
                }

                ranges.emplace_back (layer.minVelocity, layer.maxVelocity);
            }

            std::sort (ranges.begin (), ranges.end ());

            int expectedMinimum = minimumMidiVelocity;

            for (const auto& [minimum, maximum] : ranges)
            {
                if (minimum != expectedMinimum)
                {
                    return fail (
                        minimum > expectedMinimum
                            ? padName + ": no layer covers velocity " +
                                  juce::String (expectedMinimum) + ".." + juce::String (minimum - 1)
                            : padName + ": velocity layers overlap at " + juce::String (minimum) +
                                  ".." + juce::String (expectedMinimum - 1));
                }

                expectedMinimum = maximum + 1;
            }

            if (expectedMinimum <= maximumMidiVelocity)
                return fail (padName + ": no layer covers velocity " +
                             juce::String (expectedMinimum) + "..127");
        }

        return true;
    }

}
