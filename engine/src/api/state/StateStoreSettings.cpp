#include "api/StateStore.h"

#include "api/Routes.h"

#include <utility>

namespace Api
{

SettingsState StateStore::getSettings() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return settings;
}

bool StateStore::updateSettings (const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    ChangeHandler changed;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        auto next = settings;
        const auto& properties = patch.getDynamicObject()->getProperties();

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);

            if (name.toString() == "masterGain")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "masterGain", 0.0, 1.5, parsed, error))
                    return false;

                next.masterGain = parsed;
            }
            else if (name.toString() == "inputGain")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "inputGain", 0.0, 2.0, parsed, error))
                    return false;

                next.inputGain = parsed;
            }
            else if (name.toString() == "midiChannel")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "midiChannel", 1, 16, parsed, error))
                    return false;

                next.midiChannel = static_cast<int> (parsed);
            }
            else if (name.toString() == "metronome")
            {
                bool parsed = false;

                if (! detail::readBool (value, "metronome", parsed, error))
                    return false;

                next.metronome = parsed;
            }
            else
            {
                error = "unknown setting: " + name.toString().toStdString();
                return false;
            }
        }

        settings = next;
        changed = changeHandler;
    }

    if (changed != nullptr)
        changed ("settings", -1);

    return true;
}

}
