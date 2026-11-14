#include "api/MidiBridge.h"

#include "settings/MidiInputNames.h"
#include "settings/SettingsStore.h"

#include <vector>

namespace Api
{
    namespace
    {

        ihub::settings::MidiInput identityFor (const MidiInputDevice& device)
        {
            ihub::settings::MidiInput input;
            input.port = juce::String (device.name);
            input.client = device.client.empty () ? input.port : juce::String (device.client);
            return input;
        }

    }

    void MidiBridge::setSettingsStore (ihub::settings::SettingsStore* store)
    {
        {
            const std::lock_guard<std::mutex> lock (settingsMutex);
            settingsStore = store;
        }

        restoreStoredInputs ();
    }

    void MidiBridge::restoreStoredInputs ()
    {
        std::vector<ihub::settings::MidiInput> stored;

        {
            const std::lock_guard<std::mutex> lock (settingsMutex);

            if (settingsStore == nullptr)
                return;

            stored = settingsStore->settings ().midi.enabledInputs;
        }

        if (stored.empty ())
            return;

        for (const auto& device : listDevices ())
        {
            if (roleForName (juce::String (device.name)) == "launchpad")
                continue;

            if (!ihub::settings::containsMidiInput (stored, identityFor (device)))
                continue;

            {
                const std::lock_guard<std::mutex> lock (monitorMutex);
                const auto monitor = monitors.find (device.id);

                if (monitor != monitors.end () && monitor->second.input != nullptr)
                    continue;
            }

            std::string error;
            (void)openInput (device, error);
        }
    }

    void MidiBridge::rememberInput (const MidiInputDevice& device)
    {
        const std::lock_guard<std::mutex> lock (settingsMutex);

        if (settingsStore == nullptr)
            return;

        auto& inputs = settingsStore->settings ().midi.enabledInputs;
        const auto identity = identityFor (device);

        if (ihub::settings::containsMidiInput (inputs, identity))
            return;

        ihub::settings::addMidiInput (inputs, identity);
        (void)settingsStore->save ();
    }

    void MidiBridge::forgetInput (const MidiInputDevice& device)
    {
        const std::lock_guard<std::mutex> lock (settingsMutex);

        if (settingsStore == nullptr)
            return;

        auto& inputs = settingsStore->settings ().midi.enabledInputs;

        if (ihub::settings::removeMidiInput (inputs, identityFor (device)))
            (void)settingsStore->save ();
    }

}
