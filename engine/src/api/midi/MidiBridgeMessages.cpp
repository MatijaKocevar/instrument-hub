#include "api/MidiBridge.h"

#include "api/midi/MidiBridgeSupport.h"

#include <utility>

namespace Api
{

bool MidiBridge::forwardMidiMessage (const juce::String& deviceName,
                                     const juce::MidiMessage& message)
{
    if (roleForName (deviceName) == "launchpad")
        return false;

    const std::lock_guard<std::mutex> sinkLock (sinkMutex);

    if (! midiSink)
        return false;

    const std::lock_guard<std::mutex> writeLock (forwardMutex);
    return midiSink (message);
}

bool MidiBridge::routeGridMessage (const std::string& id,
                                   const juce::MidiMessage& message)
{
    if (! message.isNoteOn() && ! message.isNoteOff())
        return false;

    const bool pressed = message.isNoteOn();
    const int note = message.getNoteNumber();
    std::optional<int> pad;

    {
        const std::lock_guard<std::mutex> lock (gridMutex);
        const auto grid = grids.find (id);

        if (grid == grids.end())
            return false;

        auto& entry = grid->second;

        if (entry.map.isLearning())
        {
            if (pressed && entry.map.capture (note))
                entry.learnDeadline = std::chrono::steady_clock::now()
                                      + std::chrono::milliseconds (learnTimeoutMs);

            if (entry.map.hasCompleteLearn())
            {
                entry.error.clear();
                entry.dirty = true;
            }

            return true;
        }

        pad = entry.map.padForNote (note);
    }

    if (! pad.has_value())
        return false;

    std::function<bool (int pad, bool pressed)> sink;

    {
        const std::lock_guard<std::mutex> lock (padSinkMutex);
        sink = padSink;
    }

    if (! sink)
        return false;

    return sink (*pad, pressed);
}

bool MidiBridge::processMidiMessage (const std::string& deviceId,
                                     const juce::String& deviceName,
                                     const juce::MidiMessage& message)
{
    if (roleForName (deviceName) == "launchpad")
        return false;

    bool isGrid = false;

    {
        const std::lock_guard<std::mutex> lock (gridMutex);
        isGrid = grids.find (deviceId) != grids.end();
    }

    if (isGrid)
    {
        (void) routeGridMessage (deviceId, message);
        return true;
    }

    return forwardMidiMessage (deviceName, message);
}

std::string MidiBridge::describeMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        return "note on " + std::to_string (message.getNoteNumber())
               + " vel " + std::to_string (message.getVelocity());

    if (message.isNoteOff())
        return "note off " + std::to_string (message.getNoteNumber());

    if (message.isController())
        return "CC " + std::to_string (message.getControllerNumber())
               + " = " + std::to_string (message.getControllerValue());

    if (message.isPitchWheel())
        return "pitch " + std::to_string (message.getPitchWheelValue());

    if (message.isAftertouch())
        return "aftertouch " + std::to_string (message.getNoteNumber());

    if (message.isSysEx())
        return "SysEx " + std::to_string (message.getRawDataSize()) + " bytes";

    return "message";
}

void MidiBridge::handleIncomingMidiMessage (juce::MidiInput* source,
                                            const juce::MidiMessage& message)
{
    if (source == nullptr)
        return;

    const auto id = Detail::toStd (source->getIdentifier());
    const auto name = source->getName();

    {
        const std::lock_guard<std::mutex> lock (monitorMutex);
        const auto monitor = monitors.find (id);

        if (monitor == monitors.end())
            return;

        monitor->second.events += 1;
        monitor->second.lastEvent = describeMessage (message);
    }

    processMidiMessage (id, name, message);
}

}
