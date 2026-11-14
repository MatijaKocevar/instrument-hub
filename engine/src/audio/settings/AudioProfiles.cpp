#include "audio/settings/AudioProfiles.h"

#include <algorithm>
#include <utility>

namespace AudioEngine::detail
{

    namespace
    {
        void stripSuffix (juce::String& text, const juce::String& suffix)
        {
            if (text.endsWithIgnoreCase (suffix))
                text = text.dropLastCharacters (suffix.length ()).trimEnd ();
        }

        void collapseSpaces (juce::String& text)
        {
            auto collapsed = juce::String ();

            for (const auto character : text)
            {
                if (character == ' ' || character == '\t' || character == '_')
                {
                    if (collapsed.isNotEmpty () && !collapsed.endsWithChar (' '))
                        collapsed << ' ';
                }
                else
                {
                    collapsed << character;
                }
            }

            text = collapsed.trim ();
        }
    }

    juce::String soundcardKeyFor (const juce::String& deviceName)
    {
        auto key = deviceName.trim ().toLowerCase ();
        key = key.replaceCharacter ('\t', ' ');
        key = key.replaceCharacter ('_', ' ');
        key = key.replaceCharacter (';', ' ');
        key = key.replaceCharacter (',', ' ');

        stripSuffix (key, " (analog)");
        stripSuffix (key, " (hdmi)");
        stripSuffix (key, " analog stereo");

        const int proIndex = key.lastIndexOf (" pro");

        if (proIndex >= 0)
        {
            const auto suffix = key.substring (proIndex + 4).trim ();

            if (suffix.isEmpty () || suffix.containsOnly ("0123456789"))
                key = key.substring (0, proIndex).trimEnd ();
        }

        collapseSpaces (key);
        return key;
    }

    juce::String audioProfileLabel (const juce::String& deviceType, const juce::String& deviceName)
    {
        return deviceType.equalsIgnoreCase ("JACK") || deviceName.endsWithIgnoreCase (" Pro")
                   ? "pro-audio"
                   : "standard";
    }

    const ihub::settings::AudioProfile*
    findAudioProfile (const std::vector<ihub::settings::AudioProfile>& profiles,
                      const juce::String& deviceType,
                      const juce::String& deviceName)
    {
        const auto key = soundcardKeyFor (deviceName);

        for (const auto& profile : profiles)
        {
            if (profile.soundcardKey == key)
                return &profile;
        }

        for (const auto& profile : profiles)
        {
            if (profile.deviceType == deviceType && soundcardKeyFor (profile.deviceName) == key)
                return &profile;
        }

        return nullptr;
    }

    void upsertAudioProfile (std::vector<ihub::settings::AudioProfile>& profiles,
                             ihub::settings::AudioProfile profile)
    {
        if (profile.soundcardKey.isEmpty ())
            profile.soundcardKey = soundcardKeyFor (profile.deviceName);

        const auto existing = std::find_if (profiles.begin (),
                                            profiles.end (),
                                            [&profile] (const auto& entry)
                                            { return entry.soundcardKey == profile.soundcardKey; });

        if (existing != profiles.end ())
            *existing = std::move (profile);
        else
            profiles.push_back (std::move (profile));
    }

    std::vector<int> activeChannelList (const juce::BigInteger& mask)
    {
        std::vector<int> channels;

        for (int index = 0; index < mask.getHighestBit () + 1; ++index)
            if (mask[index])
                channels.push_back (index);

        return channels;
    }

}
