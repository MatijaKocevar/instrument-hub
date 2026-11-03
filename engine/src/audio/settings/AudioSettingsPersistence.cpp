#include "audio/settings/AudioSettingsPersistence.h"

#include "audio/DeviceManager.h"
#include "settings/SettingsStore.h"

#include <cmath>

namespace AudioEngine
{

bool persistAudioSettings (ihub::settings::SettingsStore& store,
                           const DeviceManager& manager)
{
    auto& audio = store.settings().audio;
    audio.deviceType = manager.getCurrentDeviceTypeName();
    audio.deviceName = manager.getCurrentDeviceName();
    audio.sampleRate = static_cast<int> (std::lround (manager.getCurrentSampleRate()));
    audio.bufferSize = manager.getCurrentBufferSize();

    return store.save();
}

}
