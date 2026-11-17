#pragma once

namespace ihub::settings {
class SettingsStore;
}

namespace AudioEngine {

class DeviceManager;

bool persistAudioSettings(ihub::settings::SettingsStore& store,
                          const DeviceManager& manager);

}  // namespace AudioEngine
