#include "MidiBridge.h"

#include <utility>

namespace Api
{

MidiBridge::MidiBridge()
{
    inputBackend = makeJuceMidiInputBackend (*this);
}

MidiBridge::MidiBridge (std::unique_ptr<MidiInputBackend> backend)
    : inputBackend (std::move (backend))
{
}

MidiBridge::~MidiBridge()
{
    shuttingDown.store (true, std::memory_order_release);
    padTestActive.store (false, std::memory_order_release);
    padTestGeneration.fetch_add (1, std::memory_order_acq_rel);

    {
        const std::lock_guard<std::mutex> lock (padTestMutex);

        if (padTestThread.joinable())
            padTestThread.join();
    }

    launchpad.store (nullptr, std::memory_order_release);
    setMidiSink (nullptr);
    setPadSink (nullptr);
    closeInputs();
}

void MidiBridge::setLaunchpad (Controller::LaunchpadDriver* driver)
{
    launchpad.store (driver, std::memory_order_release);
}

void MidiBridge::setMidiSink (std::function<bool (const juce::MidiMessage&)> sink)
{
    const std::lock_guard<std::mutex> lock (sinkMutex);
    midiSink = std::move (sink);
}

void MidiBridge::setPadSink (std::function<bool (int pad, bool pressed)> sink)
{
    const std::lock_guard<std::mutex> lock (padSinkMutex);
    padSink = std::move (sink);
}

void MidiBridge::setRestoreHandler (RestoreHandler handler)
{
    const std::lock_guard<std::mutex> lock (restoreMutex);
    restoreHandler = std::move (handler);
}

std::vector<MidiInputDevice> MidiBridge::listDevices() const
{
    if (inputBackend == nullptr)
        return {};

    return inputBackend->list();
}

void MidiBridge::closeInputs()
{
    std::map<std::string, Monitor> closing;

    {
        const std::lock_guard<std::mutex> lock (monitorMutex);
        closing.swap (monitors);
    }

    for (auto& entry : closing)
    {
        if (entry.second.input != nullptr)
            entry.second.input->stop();
    }
}

}
