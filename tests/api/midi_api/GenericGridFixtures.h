#pragma once

#include "api/MidiBridge.h"
#include "settings/SettingsStore.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ApiTest::MidiApi::GenericGridTest
{

class SilentHandle final : public Api::MidiInputHandle
{
public:
    void start() override {}
    void stop() override {}
};

class FakeBackend final : public Api::MidiInputBackend
{
public:
    explicit FakeBackend (std::vector<Api::MidiInputDevice> devicesToUse)
        : devices (std::move (devicesToUse))
    {
    }

    std::vector<Api::MidiInputDevice> list() override { return devices; }

    std::unique_ptr<Api::MidiInputHandle> open (const std::string&) override
    {
        return std::make_unique<SilentHandle>();
    }

private:
    std::vector<Api::MidiInputDevice> devices;
};

inline std::vector<Api::MidiInputDevice> makeDevices()
{
    return {
        { "10:0", "Nano PAD USB MIDI", "Nano PAD" },
        { "10:1", "Alesis Q49 USB MIDI", "Alesis Q49" },
        { "10:2", "Launchpad Mini MK3 LPMiniMK3 MI", "Launchpad Mini MK3" }
    };
}

inline Api::MidiInputPatch rolePatch (const std::string& id, const std::string& role)
{
    Api::MidiInputPatch patch;
    patch.id = id;
    patch.hasRole = true;
    patch.role = role;
    return patch;
}

inline Api::MidiInputPatch enablePatch (const std::string& id)
{
    Api::MidiInputPatch patch;
    patch.id = id;
    patch.hasEnabled = true;
    patch.enabled = true;
    return patch;
}

inline Api::MidiInputPatch noteBasePatch (const std::string& id, int noteBase)
{
    Api::MidiInputPatch patch;
    patch.id = id;
    patch.hasNoteBase = true;
    patch.noteBase = noteBase;
    return patch;
}

inline Api::MidiInputPatch learnPatch (const std::string& id, bool learn)
{
    Api::MidiInputPatch patch;
    patch.id = id;
    patch.hasLearn = true;
    patch.learn = learn;
    return patch;
}

inline const Api::MidiInputState* findInput (const Api::MidiStateSnapshot& state,
                                             const std::string& id)
{
    const auto found = std::find_if (state.inputs.begin(), state.inputs.end(),
                                     [&id] (const Api::MidiInputState& input)
                                     {
                                         return input.id == id;
                                     });

    return found == state.inputs.end() ? nullptr : &*found;
}

inline juce::MidiMessage noteOn (int note)
{
    return juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (100));
}

inline juce::MidiMessage noteOff (int note)
{
    return juce::MidiMessage::noteOff (1, note, 0.0f);
}

inline juce::File makeSettingsDirectory()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-generic-grid-test-"
                                              + juce::Uuid().toString());
    directory.createDirectory();
    return directory;
}

}
