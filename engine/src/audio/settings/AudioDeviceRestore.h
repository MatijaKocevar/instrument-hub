#pragma once

#include "audio/DeviceManager.h"
#include "audio/settings/AudioDevicePreference.h"
#include "settings/SettingsStore.h"

#include <vector>

namespace AudioEngine {

AudioRestoreOutcome restoreAudioDevice(const ihub::settings::AudioSettings& stored,
                                       const AudioDeviceOverrides& overrides,
                                       const std::vector<DeviceInfo>& devices,
                                       const AudioDeviceOpener& opener);

}
