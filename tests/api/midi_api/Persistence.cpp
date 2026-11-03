#include "Persistence.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "api/MidiBridge.h"
#include "settings/SettingsStore.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ApiTest::MidiApi
{
namespace
{

class FakeHandle final : public Api::MidiInputHandle
{
public:
    explicit FakeHandle (std::vector<std::string>& startsToUse)
        : starts (startsToUse)
    {
    }

    void start() override
    {
        starts.push_back ("start");
    }

    void stop() override
    {
        starts.push_back ("stop");
    }

private:
    std::vector<std::string>& starts;
};

class FakeBackend final : public Api::MidiInputBackend
{
public:
    explicit FakeBackend (std::vector<Api::MidiInputDevice> devicesToUse)
        : devices (std::move (devicesToUse))
    {
    }

    std::vector<Api::MidiInputDevice> list() override
    {
        return devices;
    }

    void setDevices (std::vector<Api::MidiInputDevice> next)
    {
        devices = std::move (next);
    }

    std::unique_ptr<Api::MidiInputHandle> open (const std::string& id) override
    {
        const auto device = std::find_if (devices.begin(), devices.end(),
                                          [&id] (const Api::MidiInputDevice& candidate)
                                          {
                                              return candidate.id == id;
                                          });

        if (device == devices.end())
            return {};

        opened.push_back (id);
        return std::make_unique<FakeHandle> (starts);
    }

    const std::vector<std::string>& openedIds() const noexcept
    {
        return opened;
    }

private:
    std::vector<Api::MidiInputDevice> devices;
    std::vector<std::string> opened;
    std::vector<std::string> starts;
};

std::vector<Api::MidiInputDevice> makeDevices()
{
    return {
        { "ALSA:10:0", "Alpha Synth MIDI 1", "Alpha Synth" },
        { "ALSA:10:1", "Alpha Synth MIDI 2", "Alpha Synth" },
        { "ALSA:11:0", "Beta Keys USB MIDI", "Beta Keys" }
    };
}

int enabledCount (const Api::MidiStateSnapshot& state)
{
    return static_cast<int> (std::count_if (state.inputs.begin(), state.inputs.end(),
                                            [] (const Api::MidiInputState& input)
                                            {
                                                return input.enabled;
                                            }));
}

std::vector<Api::MidiInputDevice> onlyFirstDevice()
{
    const auto devices = makeDevices();
    return { devices.front() };
}

bool hasInput (const ihub::settings::Settings& settings,
               const juce::String& client,
               const juce::String& port)
{
    return std::any_of (settings.midi.enabledInputs.begin(),
                        settings.midi.enabledInputs.end(),
                        [&client, &port] (const ihub::settings::MidiInput& input)
                        {
                            return input.client == client && input.port == port;
                        });
}

}

void runPersistence()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-midi-persistence-test-"
                                              + juce::Uuid().toString());
    directory.createDirectory();

    const auto file = directory.getChildFile ("settings.json");
    const auto devices = makeDevices();

    {
        ihub::settings::SettingsStore store (file);
        check (store.load() == ihub::settings::LoadStatus::missing,
               "the MIDI persistence test starts without a settings file");

        auto backend = std::make_unique<FakeBackend> (devices);
        auto& fake = *backend;
        Api::MidiBridge bridge (std::move (backend));
        bridge.setSettingsStore (&store);

        std::string error;

        check (bridge.setInputEnabled ("ALSA:10:0", true, error),
               "enabling the first synthetic input succeeds: " + error);
        check (bridge.setInputEnabled ("ALSA:11:0", true, error),
               "enabling the second synthetic input succeeds: " + error);
        check (fake.openedIds().size() == 2,
               "the bridge opens exactly the two enabled inputs");

        const auto state = bridge.snapshot();
        check (enabledCount (state) == 2,
               "the enabled inputs are reported by the snapshot");
        check (state.inputs.size() == 3 && ! state.inputs[1].enabled,
               "the input that was not enabled stays off");

        const auto text = file.loadFileAsString();
        check (! text.contains ("ALSA:10:0") && ! text.contains ("ALSA:11:0"),
               "the settings file never stores a numeric ALSA identifier");

        const auto midi = property (parseJson (text.toStdString()), "midi");
        const auto* inputs = property (midi, "enabledInputs").getArray();

        check (inputs != nullptr && inputs->size() == 2,
               "the settings file stores two enabled inputs");

        if (inputs != nullptr && inputs->size() == 2)
        {
            check (stringProperty ((*inputs)[0], "client") == "Alpha Synth"
                       && stringProperty ((*inputs)[0], "port") == "Alpha Synth MIDI 1",
                   "the first input is stored as its client and port names");
            check (stringProperty ((*inputs)[1], "client") == "Beta Keys"
                       && stringProperty ((*inputs)[1], "port") == "Beta Keys USB MIDI",
                   "the second input is stored as its client and port names");
        }
    }

    {
        ihub::settings::SettingsStore store (file);
        check (store.load() == ihub::settings::LoadStatus::loaded,
               "the MIDI settings file reloads");

        auto backend = std::make_unique<FakeBackend> (devices);
        auto& fake = *backend;
        Api::MidiBridge bridge (std::move (backend));
        bridge.setSettingsStore (&store);

        const auto state = bridge.snapshot();

        check (fake.openedIds().size() == 2,
               "the reconstructed bridge reopens only the two stored inputs");
        check (enabledCount (state) == 2,
               "the reconstructed bridge enables only the two stored inputs");
        check (state.inputs.size() == 3
                   && state.inputs[0].enabled
                   && ! state.inputs[1].enabled
                   && state.inputs[2].enabled,
               "the restored inputs match the stored client/port pairs");

        std::string error;

        check (bridge.setInputEnabled ("ALSA:11:0", false, error),
               "disabling a restored input succeeds: " + error);
        check (! hasInput (store.settings(), "Beta Keys", "Beta Keys USB MIDI"),
               "disabling an input removes its stored pair");
        check (! file.loadFileAsString().contains ("Beta Keys USB MIDI"),
               "disabling an input removes it from the settings file");

        const auto disabled = bridge.snapshot();
        check (disabled.inputs.size() == 3 && ! disabled.inputs[2].enabled,
               "the disabled input is reported off");
    }

    {
        ihub::settings::SettingsStore store (file);
        check (store.load() == ihub::settings::LoadStatus::loaded,
               "the MIDI settings file reloads for the missing-port check");

        store.settings().midi.enabledInputs.push_back (
            { "Gamma Synth", "Gamma Synth MIDI 1" });
        check (store.save(), "the stored settings gain an absent input");

        auto backend = std::make_unique<FakeBackend> (onlyFirstDevice());
        auto& fake = *backend;
        Api::MidiBridge bridge (std::move (backend));
        bridge.setSettingsStore (&store);

        const auto state = bridge.snapshot();

        check (fake.openedIds().size() == 1,
               "only the present stored input is opened");
        check (enabledCount (state) == 1,
               "a stored input missing from the enumeration stays disabled");
        check (hasInput (store.settings(), "Gamma Synth", "Gamma Synth MIDI 1"),
               "the missing input stays in the stored settings");
        check (file.loadFileAsString().contains ("Gamma Synth MIDI 1"),
               "the missing input stays in the settings file");

        auto returning = onlyFirstDevice();
        returning.push_back ({ "ALSA:12:0", "Gamma Synth MIDI 1", "Gamma Synth" });
        fake.setDevices (std::move (returning));

        const auto restored = bridge.snapshot();

        check (fake.openedIds().size() == 2,
               "the returning input is opened when it is enumerated again");
        check (restored.inputs.size() == 2 && restored.inputs[1].enabled,
               "the returning input is enabled again from the stored pair");
        check (hasInput (store.settings(), "Gamma Synth", "Gamma Synth MIDI 1"),
               "the returning input keeps its stored pair");
    }

    directory.deleteRecursively();
}

}
