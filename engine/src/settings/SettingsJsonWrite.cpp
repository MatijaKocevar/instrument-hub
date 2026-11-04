#include "settings/SettingsJson.h"

#include "settings/SettingsJsonAudio.h"

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

    juce::Array<juce::var> gridInputs;

    for (const auto& input : midi.gridInputs)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("client", input.client);
        entry->setProperty ("port", input.port);

        if (input.noteBase >= 0)
            entry->setProperty ("noteBase", input.noteBase);

        if (! input.notes.empty())
        {
            juce::Array<juce::var> notes;

            for (const int note : input.notes)
                notes.add (note);

            entry->setProperty ("notes", juce::var (notes));
        }

        gridInputs.add (juce::var (entry));
    }

    object.setProperty ("gridInputs", juce::var (gridInputs));
}

void writeAccount (juce::DynamicObject& object, const AccountSettings& account)
{
    object.setProperty ("lanEnabled", account.lanEnabled);

    if (account.pairingToken.isNotEmpty())
        object.setProperty ("pairingToken", account.pairingToken);
    else
        object.removeProperty ("pairingToken");
}

void writeProject (juce::DynamicObject& object, const ProjectSettings& project)
{
    auto& projectObject = objectFor (object, "project");
    projectObject.setProperty ("lastPath", project.lastPath);

    juce::Array<juce::var> recent;

    for (const auto& entry : project.recent)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("path", entry.path);
        item->setProperty ("name", entry.name);
        recent.add (juce::var (item));
    }

    projectObject.setProperty ("recent", juce::var (recent));
}

}

juce::var buildDocument (const juce::var& preserved, const Settings& settings)
{
    auto root = preserved.getDynamicObject() != nullptr
                    ? preserved.clone()
                    : juce::var (new juce::DynamicObject());

    auto* rootObject = root.getDynamicObject();
    rootObject->setProperty ("version", schemaVersion);

    writeAudioSettings (objectFor (*rootObject, "audio"), settings.audio);
    writeMixer (objectFor (*rootObject, "mixer"), settings.mixer);
    writeMidi (objectFor (*rootObject, "midi"), settings.midi);
    writeAccount (objectFor (*rootObject, "account"), settings.account);
    writeProject (*rootObject, settings.project);

    return root;
}

}
