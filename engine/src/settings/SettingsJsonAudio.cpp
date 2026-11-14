#include "settings/SettingsJsonAudio.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ihub::settings::detail
{
    namespace
    {

        bool
        readNumber (const juce::var& value, double minimum, double maximum, double& destination)
        {
            if (!value.isInt () && !value.isInt64 () && !value.isDouble ())
                return false;

            const auto number = static_cast<double> (value);

            if (!std::isfinite (number) || number < minimum || number > maximum)
                return false;

            destination = number;
            return true;
        }

        bool readInteger (const juce::var& value, int minimum, int maximum, int& destination)
        {
            if (!value.isInt () && !value.isInt64 () && !value.isDouble ())
                return false;

            const auto number = static_cast<double> (value);

            if (!std::isfinite (number) || std::abs (number - std::floor (number)) > 0.0 ||
                number < static_cast<double> (minimum) || number > static_cast<double> (maximum))
                return false;

            destination = static_cast<int> (number);
            return true;
        }

        bool readString (const juce::var& value, juce::String& destination)
        {
            if (!value.isString ())
                return false;

            destination = value.toString ();
            return true;
        }

        void readChannels (const juce::var& value, std::vector<int>& destination)
        {
            const auto* array = value.getArray ();

            if (array == nullptr)
                return;

            destination.clear ();

            for (const auto& item : *array)
            {
                int channel = 0;

                if (!readInteger (item, 0, 63, channel))
                    continue;

                if (std::find (destination.begin (), destination.end (), channel) ==
                    destination.end ())
                    destination.push_back (channel);
            }
        }

        AudioProfile readProfile (const juce::var& value)
        {
            AudioProfile profile;
            const auto* object = value.getDynamicObject ();

            if (object == nullptr)
                return profile;

            readString (object->getProperty ("soundcardKey"), profile.soundcardKey);
            readString (object->getProperty ("deviceType"), profile.deviceType);
            readString (object->getProperty ("deviceName"), profile.deviceName);
            readInteger (object->getProperty ("sampleRate"), 0, 768000, profile.sampleRate);
            readInteger (object->getProperty ("bufferSize"), 0, 32768, profile.bufferSize);
            readChannels (object->getProperty ("inputChannels"), profile.inputChannels);
            readChannels (object->getProperty ("outputChannels"), profile.outputChannels);
            readString (object->getProperty ("profile"), profile.profile);
            readNumber (object->getProperty ("latencyMs"), 0.0, 10000.0, profile.latencyMs);
            readNumber (object->getProperty ("measuredMs"), 0.0, 10000.0, profile.measuredMs);
            readInteger (object->getProperty ("xruns"), 0, 1000000000, profile.xruns);

            if (profile.soundcardKey.isEmpty ())
                profile.soundcardKey = profile.deviceName;

            return profile;
        }

        void readProfiles (const juce::var& value, AudioSettings& audio)
        {
            const auto* array = value.getArray ();

            if (array == nullptr)
                return;

            audio.profiles.clear ();

            for (const auto& item : *array)
            {
                auto profile = readProfile (item);

                if (profile.soundcardKey.isEmpty () || profile.deviceName.isEmpty ())
                    continue;

                const auto existing =
                    std::find_if (audio.profiles.begin (),
                                  audio.profiles.end (),
                                  [&profile] (const AudioProfile& entry)
                                  { return entry.soundcardKey == profile.soundcardKey; });

                if (existing != audio.profiles.end ())
                    *existing = std::move (profile);
                else
                    audio.profiles.push_back (std::move (profile));

                if (audio.profiles.size () >= 64)
                    break;
            }
        }

        juce::var channelsToVar (const std::vector<int>& channels)
        {
            juce::Array<juce::var> result;

            for (const auto channel : channels)
                result.add (channel);

            return juce::var (result);
        }

        juce::var profileToVar (const AudioProfile& profile)
        {
            auto* object = new juce::DynamicObject ();
            object->setProperty ("soundcardKey", profile.soundcardKey);
            object->setProperty ("deviceType", profile.deviceType);
            object->setProperty ("deviceName", profile.deviceName);
            object->setProperty ("sampleRate", profile.sampleRate);
            object->setProperty ("bufferSize", profile.bufferSize);
            object->setProperty ("inputChannels", channelsToVar (profile.inputChannels));
            object->setProperty ("outputChannels", channelsToVar (profile.outputChannels));
            object->setProperty ("profile", profile.profile);
            object->setProperty ("latencyMs", profile.latencyMs);
            object->setProperty ("measuredMs", profile.measuredMs);
            object->setProperty ("xruns", profile.xruns);
            return juce::var (object);
        }

    }

    void readAudioSettings (const juce::var& value, AudioSettings& audio)
    {
        const auto* object = value.getDynamicObject ();

        if (object == nullptr)
            return;

        readString (object->getProperty ("deviceType"), audio.deviceType);
        readString (object->getProperty ("deviceName"), audio.deviceName);
        readInteger (object->getProperty ("sampleRate"), 1, 768000, audio.sampleRate);
        readInteger (object->getProperty ("bufferSize"), 1, 32768, audio.bufferSize);
        readProfiles (object->getProperty ("profiles"), audio);
    }

    void writeAudioSettings (juce::DynamicObject& object, const AudioSettings& audio)
    {
        object.setProperty ("deviceType", audio.deviceType);
        object.setProperty ("deviceName", audio.deviceName);
        object.setProperty ("sampleRate", audio.sampleRate);
        object.setProperty ("bufferSize", audio.bufferSize);

        juce::Array<juce::var> profiles;

        for (const auto& profile : audio.profiles)
            profiles.add (profileToVar (profile));

        object.setProperty ("profiles", juce::var (profiles));
    }

}
