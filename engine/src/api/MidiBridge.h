#pragma once

#include "Server.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Controller
{
class LaunchpadDriver;
}

namespace Api
{

class MidiBridge final : private juce::MidiInputCallback
{
public:
    static constexpr int padTestDurationMs = 900;

    MidiBridge();
    ~MidiBridge() override;

    void setLaunchpad (Controller::LaunchpadDriver* driver);

    void setMidiSink (std::function<bool (const juce::MidiMessage&)> sink);

    bool forwardMidiMessage (const juce::String& deviceName,
                             const juce::MidiMessage& message);

    using RestoreHandler = std::function<void()>;
    void setRestoreHandler (RestoreHandler handler);

    MidiStateSnapshot snapshot();

    bool setInputEnabled (const std::string& id, bool enabled, std::string& error);

    bool runPadTest (std::string& error);

    bool isPadTestActive() const noexcept;

    static std::string roleForName (const juce::String& name);

    static std::string portForName (const juce::String& name);

    static std::vector<MidiInputState> enumerateInputs();

private:
    struct Monitor
    {
        std::unique_ptr<juce::MidiInput> input;
        std::uint64_t events = 0;
        std::string lastEvent;
    };

    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void paintPadTestFrame (int frame);
    void runPadTestAnimation (std::uint64_t generation);
    void closeInputs();

    static std::string describeMessage (const juce::MidiMessage& message);

    std::atomic<Controller::LaunchpadDriver*> launchpad { nullptr };

    mutable std::mutex sinkMutex;
    std::function<bool (const juce::MidiMessage&)> midiSink;

    std::mutex forwardMutex;

    mutable std::mutex monitorMutex;
    std::map<std::string, Monitor> monitors;

    std::mutex padTestMutex;
    std::thread padTestThread;
    std::atomic<bool> padTestActive { false };
    std::atomic<bool> shuttingDown { false };
    std::atomic<int> padTestLit { 0 };
    std::atomic<std::uint64_t> padTestGeneration { 0 };

    mutable std::mutex restoreMutex;
    RestoreHandler restoreHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiBridge)
};

}
