#include "audio/fx/FxInsertDriveParameters.h"

#include <cmath>

namespace AudioEngine {

bool applyDriveParameters(ihub::dsp::Drive& drive, const std::map<std::string, float>& parameters,
                          std::string& error) {
    auto settings = drive.getSettings();

    for (const auto& [key, value] : parameters) {
        if (key == "model") {
            const int model = static_cast<int>(std::lround(value));

            if (model < 0 || model >= ihub::dsp::Drive::numModels) {
                error = "drive model must be 0.." + std::to_string(ihub::dsp::Drive::numModels - 1);
                return false;
            }

            settings.model = model;
        } else if (key == "drive")
            settings.drive =
                juce::jlimit(ihub::dsp::Drive::minDrive, ihub::dsp::Drive::maxDrive, value);
        else if (key == "tone")
            settings.tone =
                juce::jlimit(ihub::dsp::Drive::minTone, ihub::dsp::Drive::maxTone, value);
        else if (key == "levelDb" || key == "level")
            settings.levelDb =
                juce::jlimit(ihub::dsp::Drive::minLevelDb, ihub::dsp::Drive::maxLevelDb, value);
        else if (key == "cab")
            settings.cab = value >= 0.5f;
        else {
            error = "unknown drive parameter \"" + key + "\"";
            return false;
        }
    }

    drive.setSettings(settings);
    return true;
}

std::map<std::string, float> readDriveParameters(const ihub::dsp::Drive& drive) {
    const auto settings = drive.getSettings();

    return {{"model", static_cast<float>(settings.model)},
            {"drive", settings.drive},
            {"tone", settings.tone},
            {"levelDb", settings.levelDb},
            {"cab", settings.cab ? 1.0f : 0.0f}};
}

}  // namespace AudioEngine
