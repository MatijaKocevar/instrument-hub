#include "settings/SettingsJson.h"

#include <cmath>
#include <utility>

namespace ihub::settings::detail
{
namespace
{

bool readNumber (const juce::var& value, double minimum, double maximum, double& destination)
{
    if (! value.isInt() && ! value.isInt64() && ! value.isDouble())
        return false;

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number) || number < minimum || number > maximum)
        return false;

    destination = number;
    return true;
}

bool readInteger (const juce::var& value, int minimum, int maximum, int& destination)
{
    if (! value.isInt() && ! value.isInt64() && ! value.isDouble())
        return false;

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number)
        || std::abs (number - std::floor (number)) > 0.0
        || number < static_cast<double> (minimum)
        || number > static_cast<double> (maximum))
        return false;

    destination = static_cast<int> (number);
    return true;
}

bool readString (const juce::var& value, juce::String& destination)
{
    if (! value.isString())
        return false;

    destination = value.toString();
    return true;
}

void readAudio (const juce::var& value, AudioSettings& audio)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
        return;

    readString (object->getProperty ("deviceType"), audio.deviceType);
    readString (object->getProperty ("deviceName"), audio.deviceName);
    readInteger (object->getProperty ("sampleRate"), 1, 768000, audio.sampleRate);
    readInteger (object->getProperty ("bufferSize"), 1, 32768, audio.bufferSize);
}

void readMixer (const juce::var& value, MixerSettings& mixer)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
        return;

    readNumber (object->getProperty ("masterGain"), 0.0, 1.5, mixer.masterGain);
    readNumber (object->getProperty ("inputGain"), 0.0, 2.0, mixer.inputGain);
    readInteger (object->getProperty ("midiChannel"), 1, 16, mixer.midiChannel);

    const auto metronome = object->getProperty ("metronome");

    if (metronome.isBool())
        mixer.metronome = static_cast<bool> (metronome);
}

void readInputs (const juce::var& value, MidiSettings& midi)
{
    const auto* array = value.getArray();

    if (array == nullptr)
        return;

    midi.enabledInputs.clear();

    for (const auto& item : *array)
    {
        const auto* object = item.getDynamicObject();

        if (object == nullptr)
            continue;

        MidiInput input;

        if (! readString (object->getProperty ("client"), input.client)
            || ! readString (object->getProperty ("port"), input.port))
            continue;

        if (input.client.isNotEmpty() && input.port.isNotEmpty())
            midi.enabledInputs.push_back (std::move (input));
    }
}

void readAccount (const juce::var& value, AccountSettings& account)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
        return;

    const auto lanEnabled = object->getProperty ("lanEnabled");

    if (lanEnabled.isBool())
        account.lanEnabled = static_cast<bool> (lanEnabled);

    readString (object->getProperty ("pairingToken"), account.pairingToken);
}

}

void applyDocument (const juce::var& document, Settings& settings)
{
    const auto* root = document.getDynamicObject();

    if (root == nullptr)
        return;

    readAudio (root->getProperty ("audio"), settings.audio);
    readMixer (root->getProperty ("mixer"), settings.mixer);

    const auto midi = root->getProperty ("midi");

    if (const auto* midiObject = midi.getDynamicObject())
        readInputs (midiObject->getProperty ("enabledInputs"), settings.midi);

    readAccount (root->getProperty ("account"), settings.account);
}

}
