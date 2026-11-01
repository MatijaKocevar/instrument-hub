#include "pad_audio/Rig.h"

#include <filesystem>

namespace PadAudio
{

bool PadRig::load (const std::string& file, std::string& errorText)
{
    Controller::Mapping::Listeners mappingListeners;

    mappingListeners.armSlot = [this] (int slot)
    {
        armedRequests.push_back (slot);

        if (slot >= 0)
            session.armSlot (slot);
    };

    mapping.setListeners (std::move (mappingListeners));

    Controller::MappingError error;

    if (! mapping.load (std::filesystem::path (file), &error))
    {
        errorText = error.message;
        return false;
    }

    Controller::LaunchpadDriver::DriverListeners driverListeners;

    driverListeners.padEvent = [this] (int pad, bool pressed)
    {
        mapping.handlePad (pad, pressed);
    };

    driver.setListeners (std::move (driverListeners));
    return true;
}

void PadRig::press (int pad)
{
    const int note = Launchpad::padNoteForIndex (pad);

    driver.injectMidiMessage (juce::MidiMessage::noteOn (1, note,
                                                         static_cast<juce::uint8> (127)));
    driver.injectMidiMessage (juce::MidiMessage::noteOn (1, note,
                                                         static_cast<juce::uint8> (0)));
}

} // namespace PadAudio
