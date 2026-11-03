#include "settings/SettingsJson.h"

namespace ihub::settings::detail
{
namespace
{

juce::DynamicObject& objectFor (juce::DynamicObject& parent, const juce::Identifier& name)
{
    auto child = parent.getProperty (name);

    if (auto* object = child.getDynamicObject())
        return *object;

    parent.setProperty (name, juce::var (new juce::DynamicObject()));
    return *parent.getProperty (name).getDynamicObject();
}

void writeAudio (juce::DynamicObject& object, const AudioSettings& audio)
{
    object.setProperty ("deviceType", audio.deviceType);
    object.setProperty ("deviceName", audio.deviceName);
    object.setProperty ("sampleRate", audio.sampleRate);
    object.setProperty ("bufferSize", audio.bufferSize);
}

void writeMixer (juce::DynamicObject& object, const MixerSettings& mixer)
{
    object.setProperty ("masterGain", mixer.masterGain);
    object.setProperty ("inputGain", mixer.inputGain);
    object.setProperty ("midiChannel", mixer.midiChannel);
    object.setProperty ("metronome", mixer.metronome);
}

void writeMidi (juce::DynamicObject& object, const MidiSettings& midi)
{
    juce::Array<juce::var> inputs;

    for (const auto& input : midi.enabledInputs)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("client", input.client);
        entry->setProperty ("port", input.port);
        inputs.add (juce::var (entry));
    }

    object.setProperty ("enabledInputs", juce::var (inputs));
}

void writeAccount (juce::DynamicObject& object, const AccountSettings& account)
{
    object.setProperty ("lanEnabled", account.lanEnabled);

    if (account.pairingToken.isNotEmpty())
        object.setProperty ("pairingToken", account.pairingToken);
    else
        object.removeProperty ("pairingToken");
}

}

juce::var buildDocument (const juce::var& preserved, const Settings& settings)
{
    auto root = preserved.getDynamicObject() != nullptr
                    ? preserved.clone()
                    : juce::var (new juce::DynamicObject());

    auto* rootObject = root.getDynamicObject();
    rootObject->setProperty ("version", schemaVersion);

    writeAudio (objectFor (*rootObject, "audio"), settings.audio);
    writeMixer (objectFor (*rootObject, "mixer"), settings.mixer);
    writeMidi (objectFor (*rootObject, "midi"), settings.midi);
    writeAccount (objectFor (*rootObject, "account"), settings.account);

    return root;
}

}
