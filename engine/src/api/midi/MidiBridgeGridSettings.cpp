#include "api/MidiBridge.h"

#include "settings/MidiInputNames.h"
#include "settings/SettingsStore.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace Api {

namespace {
struct GridSettingUpdate {
    juce::String client;
    juce::String port;
    int noteBase = -1;
    std::vector<int> notes;
};

ihub::settings::MidiInput identityFor(const MidiInputDevice& device) {
    ihub::settings::MidiInput input;
    input.port = juce::String(device.name);
    input.client = device.client.empty() ? input.port : juce::String(device.client);
    return input;
}

bool findGridSetting(const std::vector<ihub::settings::MidiGridInput>& inputs,
                     const ihub::settings::MidiInput& identity,
                     ihub::settings::MidiGridInput& destination) {
    const auto found = std::find_if(
        inputs.begin(), inputs.end(), [&identity](const ihub::settings::MidiGridInput& input) {
            return ihub::settings::sameMidiInput({input.client, input.port}, identity);
        });

    if (found == inputs.end()) return false;

    destination = *found;
    return true;
}
}  // namespace

void MidiBridge::restoreGridConfigs() {
    if (settingsStore == nullptr) return;

    std::vector<ihub::settings::MidiGridInput> stored;

    {
        const std::lock_guard<std::mutex> lock(settingsMutex);
        stored = settingsStore->settings().midi.gridInputs;
    }

    if (stored.empty()) return;

    for (const auto& device : listDevices()) {
        if (roleForName(juce::String(device.name)) == "launchpad") continue;

        ihub::settings::MidiGridInput setting;

        if (!findGridSetting(stored, identityFor(device), setting)) continue;

        const std::lock_guard<std::mutex> lock(gridMutex);

        if (grids.find(device.id) != grids.end()) continue;

        auto& entry = grids[device.id];
        entry.client = juce::String(device.client);
        entry.port = juce::String(device.name);

        if (!setting.notes.empty()) {
            if (!entry.map.restore(setting.notes)) entry.map.setNoteBase(setting.noteBase);
        } else {
            entry.map.setNoteBase(setting.noteBase);
        }
    }
}

void MidiBridge::persistGridSettings() {
    if (settingsStore == nullptr) return;

    std::vector<GridSettingUpdate> updates;

    {
        const std::lock_guard<std::mutex> lock(gridMutex);

        for (auto& [id, entry] : grids) {
            if (!entry.dirty) continue;

            updates.push_back(
                {entry.client, entry.port, entry.map.noteBase(),
                 entry.map.hasCompleteLearn() ? entry.map.learnedNotes() : std::vector<int>{}});
            entry.dirty = false;
        }
    }

    if (updates.empty()) return;

    const std::lock_guard<std::mutex> lock(settingsMutex);
    auto& stored = settingsStore->settings().midi.gridInputs;

    for (const auto& update : updates) {
        const ihub::settings::MidiInput identity{update.client, update.port};
        auto found = std::find_if(
            stored.begin(), stored.end(), [&identity](const ihub::settings::MidiGridInput& input) {
                return ihub::settings::sameMidiInput({input.client, input.port}, identity);
            });

        if (found == stored.end()) {
            ihub::settings::MidiGridInput created;
            created.client = update.client;
            created.port = update.port;
            stored.push_back(std::move(created));
            found = std::prev(stored.end());
        }

        found->noteBase = update.noteBase;
        found->notes = update.notes;
    }

    (void)settingsStore->save();
}

void MidiBridge::removeGridSetting(const juce::String& client, const juce::String& port) {
    if (settingsStore == nullptr) return;

    const std::lock_guard<std::mutex> lock(settingsMutex);
    auto& stored = settingsStore->settings().midi.gridInputs;
    const ihub::settings::MidiInput identity{client, port};
    const auto previous = stored.size();

    stored.erase(std::remove_if(stored.begin(), stored.end(),
                                [&identity](const ihub::settings::MidiGridInput& input) {
                                    return ihub::settings::sameMidiInput({input.client, input.port},
                                                                         identity);
                                }),
                 stored.end());

    if (stored.size() != previous) (void)settingsStore->save();
}

void MidiBridge::expireLearnTimeouts() {
    const auto now = std::chrono::steady_clock::now();
    const std::lock_guard<std::mutex> lock(gridMutex);

    for (auto& [id, entry] : grids) {
        if (!entry.map.isLearning() || now < entry.learnDeadline) continue;

        const int captured = entry.map.learnedCount();
        entry.map.cancelLearn();
        entry.error = captured == 0 ? "no pad notes arrived; a grid controller must send notes"
                                    : "only " + std::to_string(captured) +
                                          " of 64 pads arrived before the learn timeout";
        entry.dirty = true;
    }
}

}  // namespace Api
