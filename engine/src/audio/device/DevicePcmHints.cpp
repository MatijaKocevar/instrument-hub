#include "DevicePcmHints.h"

#if defined(__linux__)
#include <alsa/asoundlib.h>
#endif

#include <cstdlib>
#include <utility>

namespace AudioEngine::detail
{

    std::vector<PcmHint> readAlsaPcmHints ()
    {
        std::vector<PcmHint> result;

#if defined(__linux__)
        void** hints = nullptr;

        if (snd_device_name_hint (-1, "pcm", &hints) < 0)
            return result;

        for (char** entry = reinterpret_cast<char**> (hints); *entry != nullptr; ++entry)
        {
            char* name = snd_device_name_get_hint (*entry, "NAME");
            char* description = snd_device_name_get_hint (*entry, "DESC");

            if (name != nullptr)
            {
                PcmHint hint;
                hint.name = name;
                hint.description =
                    description != nullptr ? juce::String (description) : juce::String ();
                result.push_back (std::move (hint));
            }

            std::free (name);
            std::free (description);
        }

        snd_device_name_free_hint (hints);
#endif

        return result;
    }

    juce::String juceDeviceNameForPcm (const std::vector<PcmHint>& hints,
                                       const juce::String& pcmName)
    {
        for (const auto& hint : hints)
            if (hint.name == pcmName)
                return hint.description.replace ("\n", "; ");

        return {};
    }

    juce::String pcmNameForJuceDescription (const std::vector<PcmHint>& hints,
                                            const juce::String& juceName)
    {
        for (const auto& hint : hints)
            if (hint.description.replace ("\n", "; ") == juceName)
                return hint.name;

        return {};
    }

} // namespace AudioEngine::detail
