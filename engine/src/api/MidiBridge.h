#pragma once

#include "Server.h"
#include "api/midi/MidiGridMap.h"
#include "api/midi/MidiInputBackend.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Controller {
class LaunchpadDriver;
}

namespace ihub::settings {
class SettingsStore;
}

namespace Api {

class MidiBridge final : private juce::MidiInputCallback {
public:
    static constexpr int learnTimeoutMs = 10000;

    MidiBridge();
    explicit MidiBridge(std::unique_ptr<MidiInputBackend> backend);
    ~MidiBridge() override;

    void setLaunchpad(Controller::LaunchpadDriver* driver);

    void setSettingsStore(ihub::settings::SettingsStore* store);

    void setMidiSink(std::function<bool(const juce::MidiMessage&)> sink);

    void setPadSink(std::function<bool(int pad, bool pressed)> sink);

    bool forwardMidiMessage(const juce::String& deviceName, const juce::MidiMessage& message);

    bool sendToSession(const juce::MidiMessage& message);

    bool processMidiMessage(const std::string& deviceId, const juce::String& deviceName,
                            const juce::MidiMessage& message);

    static bool isNetworkId(const std::string& id);

    void attachNetworkClient(const std::string& id, const std::string& name, std::uint64_t session);

    void detachNetworkClient(const std::string& id, std::uint64_t session);

    bool networkMidi(const std::string& id, std::uint64_t session, const std::uint8_t* data,
                     int size);

    using RestoreHandler = std::function<void()>;
    void setRestoreHandler(RestoreHandler handler);

    MidiStateSnapshot snapshot();

    bool updateInput(const MidiInputPatch& patch, std::string& error);

    bool setInputEnabled(const std::string& id, bool enabled, std::string& error);

    static std::string roleForName(const juce::String& name);

    static std::string portForName(const juce::String& name);

    static std::vector<MidiInputState> enumerateInputs();

private:
    struct Monitor {
        std::unique_ptr<MidiInputHandle> input;
        std::uint64_t events = 0;
        std::string lastEvent;
    };

    struct GridEntry {
        MidiGridMap map;
        juce::String client;
        juce::String port;
        std::string error;
        bool dirty = false;
        std::chrono::steady_clock::time_point learnDeadline{};
    };

    struct NetworkClient {
        static constexpr int noteCount = 16 * 128;

        std::uint64_t session = 0;
        std::string name;
        bool enabled = true;
        bool connected = false;
        std::uint64_t events = 0;
        std::string lastEvent;
        std::array<bool, noteCount> notes{};
        std::uint16_t channels = 0;
    };

    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override;

    std::vector<MidiInputDevice> listDevices() const;
    bool openInput(const MidiInputDevice& device, std::string& error);
    void restoreStoredInputs();
    void rememberInput(const MidiInputDevice& device);
    void forgetInput(const MidiInputDevice& device);

    MidiInputState makeState(const MidiInputDevice& device) const;
    MidiInputState makeNetworkState(const MidiInputDevice& device) const;
    void applyGridState(MidiInputState& state, const std::string& id) const;
    void appendNetworkInputs(MidiStateSnapshot& state) const;
    std::optional<MidiInputDevice> findNetworkDevice(const std::string& id) const;
    bool setNetworkEnabled(const std::string& id, bool enabled);
    void releaseNetworkNotes(std::array<bool, NetworkClient::noteCount> notes,
                             std::uint16_t channels);
    bool routeGridMessage(const std::string& id, const juce::MidiMessage& message);
    void restoreGridConfigs();
    void persistGridSettings();
    void removeGridSetting(const juce::String& client, const juce::String& port);
    void expireLearnTimeouts();

    void closeInputs();

    static std::string describeMessage(const juce::MidiMessage& message);

    std::unique_ptr<MidiInputBackend> inputBackend;
    ihub::settings::SettingsStore* settingsStore = nullptr;

    std::atomic<Controller::LaunchpadDriver*> launchpad{nullptr};

    mutable std::mutex sinkMutex;
    std::function<bool(const juce::MidiMessage&)> midiSink;

    mutable std::mutex padSinkMutex;
    std::function<bool(int pad, bool pressed)> padSink;

    std::mutex forwardMutex;

    mutable std::mutex monitorMutex;
    std::map<std::string, Monitor> monitors;

    mutable std::mutex gridMutex;
    std::map<std::string, GridEntry> grids;

    mutable std::mutex networkMutex;
    std::map<std::string, NetworkClient> networkClients;

    mutable std::mutex settingsMutex;

    mutable std::mutex restoreMutex;
    RestoreHandler restoreHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBridge)
};

}  // namespace Api
