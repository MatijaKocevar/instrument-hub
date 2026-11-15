#pragma once

#include <juce_events/juce_events.h>

#include <string>
#include <vector>

namespace Api
{
    class StateStore;
}

namespace ihub::app
{

    class MidiDeviceWatcher final : private juce::Timer
    {
    public:
        static constexpr int pollIntervalMs = 1500;

        explicit MidiDeviceWatcher (Api::StateStore& stateToUse);
        ~MidiDeviceWatcher () override;

        MidiDeviceWatcher (const MidiDeviceWatcher&) = delete;
        MidiDeviceWatcher& operator= (const MidiDeviceWatcher&) = delete;

    private:
        void timerCallback () override;

        Api::StateStore& state;
        std::vector<std::string> knownIds;
    };

}
