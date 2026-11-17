#include "api/StateStore.h"

#include "settings/SettingsStore.h"

namespace Api {

ihub::settings::SettingsStore* StateStore::getSettingsStore() const noexcept {
    return settingsStore;
}

void StateStore::loadStoredSettings() {
    if (settingsStore == nullptr) return;

    const auto& mixer = settingsStore->settings().mixer;

    settings.masterGain = mixer.masterGain;
    settings.inputGain = mixer.inputGain;
    settings.midiChannel = mixer.midiChannel;
    settings.metronome = mixer.metronome;
}

bool StateStore::persistMixerSettings(const SettingsState& next, std::string& error) {
    if (settingsStore == nullptr) return true;

    auto& mixer = settingsStore->settings().mixer;
    mixer.masterGain = next.masterGain;
    mixer.inputGain = next.inputGain;
    mixer.midiChannel = next.midiChannel;
    mixer.metronome = next.metronome;

    if (settingsStore->save()) return true;

    error = settingsStore->getLastError().isNotEmpty() ? settingsStore->getLastError().toStdString()
                                                       : "could not save the settings file";
    return false;
}

bool StateStore::persistAccountSettings(bool lanEnabled, const std::string& pairingToken) {
    if (settingsStore == nullptr) return true;

    const std::lock_guard<std::mutex> lock(mutex);

    auto& account = settingsStore->settings().account;
    account.lanEnabled = lanEnabled;
    account.pairingToken = juce::String(pairingToken);

    return settingsStore->save();
}

}  // namespace Api
