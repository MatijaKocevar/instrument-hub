#include "audio/AudioSettings.h"

#include "audio/settings/AudioProfiles.h"
#include "settings/SettingsStore.h"

#include <cmath>
#include <utility>

namespace AudioEngine {

bool AudioSettingsBridge::applyRecommendation(std::string& error) {
  Api::AudioWizardState recommendation;

  {
    const std::lock_guard<std::mutex> lock(mutex);
    recommendation = lastRecommendation;
  }

  if (!recommendation.valid) {
    error = recommendation.error.empty()
                ? "run Test & recommend before applying a profile"
                : recommendation.error;
    return false;
  }

  Api::AudioSettingsPatch patch;
  patch.deviceType = recommendation.deviceType;
  patch.deviceName = recommendation.deviceName;
  patch.sampleRate = recommendation.sampleRate;
  patch.bufferSize = recommendation.bufferSize;
  patch.inputChannels = recommendation.inputChannels;
  patch.outputChannels = recommendation.outputChannels;

  if (!apply(patch, error)) return false;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (settingsStore != nullptr) {
      auto& audio = settingsStore->settings().audio;

      ihub::settings::AudioProfile profile;
      profile.soundcardKey = recommendation.soundcard;
      profile.deviceType = recommendation.deviceType;
      profile.deviceName = recommendation.deviceName;
      profile.sampleRate =
          static_cast<int>(std::lround(recommendation.sampleRate));
      profile.bufferSize = recommendation.bufferSize;
      profile.inputChannels = recommendation.inputChannels;
      profile.outputChannels = recommendation.outputChannels;
      profile.profile = recommendation.profile;
      profile.latencyMs = recommendation.latencyMs;
      profile.measuredMs = recommendation.measuredMs;
      profile.xruns = static_cast<int>(recommendation.xruns);

      detail::upsertAudioProfile(audio.profiles, std::move(profile));

      if (!settingsStore->save()) {
        error =
            "the settings were applied but the profile could not "
            "be saved: " +
            settingsStore->getLastError().toStdString();
        return false;
      }
    }

    lastRecommendation = {};
  }

  return true;
}

}  // namespace AudioEngine
