#include "audio/settings/AudioSettingsPersistence.h"

#include "audio/DeviceManager.h"
#include "audio/settings/AudioProfiles.h"
#include "settings/SettingsStore.h"

#include <cmath>
#include <utility>

namespace AudioEngine
{

    bool persistAudioSettings (ihub::settings::SettingsStore& store, const DeviceManager& manager)
    {
        auto& audio = store.settings ().audio;
        audio.deviceType = manager.getCurrentDeviceTypeName ();
        audio.deviceName = manager.getCurrentDeviceName ();
        audio.sampleRate = static_cast<int> (std::lround (manager.getCurrentSampleRate ()));
        audio.bufferSize = manager.getCurrentBufferSize ();

        ihub::settings::AudioProfile profile;
        profile.soundcardKey = detail::soundcardKeyFor (audio.deviceName);
        profile.deviceType = audio.deviceType;
        profile.deviceName = audio.deviceName;
        profile.sampleRate = audio.sampleRate;
        profile.bufferSize = audio.bufferSize;
        profile.inputChannels = detail::activeChannelList (manager.getActiveInputChannels ());
        profile.outputChannels = detail::activeChannelList (manager.getActiveOutputChannels ());
        profile.profile = detail::audioProfileLabel (audio.deviceType, audio.deviceName);

        if (const auto* previous =
                detail::findAudioProfile (audio.profiles, audio.deviceType, audio.deviceName);
            previous != nullptr)
        {
            profile.latencyMs = previous->latencyMs;
            profile.measuredMs = previous->measuredMs;
            profile.xruns = previous->xruns;
        }

        detail::upsertAudioProfile (audio.profiles, std::move (profile));

        return store.save ();
    }

}
