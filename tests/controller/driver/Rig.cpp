#include "Rig.h"

#include <iomanip>
#include <sstream>
#include <utility>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace DriverTest
{

std::string hex (const Bytes& bytes)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill ('0');

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
            out << ' ';

        out << std::setw (2) << static_cast<unsigned int> (bytes[i]);
    }

    return out.str();
}

std::string hex (const std::vector<Bytes>& messages)
{
    std::string result;

    for (const auto& message : messages)
        result += (result.empty() ? "" : " | ") + hex (message);

    return result;
}

std::string velocityHex (std::uint8_t value)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill ('0') << std::setw (2)
        << static_cast<unsigned int> (value);
    return out.str();
}

Bytes padLed (int pad, Controller::PadState state)
{
    const auto bytes = Launchpad::padColourMessage (
        pad, Controller::launchpadColourForState (state));
    return Bytes (bytes.begin(), bytes.end());
}

Bytes buttonLed (int cc, Controller::PadState state)
{
    const auto bytes = Launchpad::buttonColourMessage (
        cc, Controller::launchpadColourForState (state));
    return Bytes (bytes.begin(), bytes.end());
}

bool Rig::load (std::string& errorText)
{
    driver.setMessageSink ([this] (const juce::MidiMessage& message)
    {
        sent.emplace_back (message.getRawData(),
                           message.getRawData() + message.getRawDataSize());
    });

    Controller::Mapping::Listeners mappingListeners;

    mappingListeners.armSlot = [this] (int slot) { armedSlots.push_back (slot); };

    mappingListeners.padEvent = [this] (int pad,
                                        const Controller::PadBinding*,
                                        bool pressed)
    {
        if (! pressed)
            return;

        if (activePad >= 0)
            driver.setPadState (activePad, Controller::PadState::idle);

        driver.setPadState (pad, Controller::PadState::warming);
        driver.setPadState (pad, Controller::PadState::active);
        activePad = pad;
    };

    mappingListeners.bankEvent = [this] (int bank, const Controller::MacroBankSpec*)
    {
        for (int index = 0; index < Launchpad::topRowCount; ++index)
            driver.setButtonState (Launchpad::topRowCCForIndex (index),
                                   index == bank ? Controller::PadState::active
                                                 : Controller::PadState::idle);
    };

    mappingListeners.sceneEvent = [this] (int scene, const Controller::SceneSpec*)
    {
        for (int index = 0; index < Launchpad::rightColumnCount; ++index)
            driver.setButtonState (Launchpad::rightColumnCCForIndex (index),
                                   index == scene ? Controller::PadState::active
                                                  : Controller::PadState::idle);
    };

    mapping.setListeners (std::move (mappingListeners));

    Controller::LaunchpadDriver::DriverListeners driverListeners;

    driverListeners.padEvent = [this] (int pad, bool pressed)
    {
        mapping.handlePad (pad, pressed);
    };

    driverListeners.buttonEvent = [this] (int cc, bool pressed)
    {
        mapping.handleButton (cc, pressed);
    };

    driver.setListeners (std::move (driverListeners));

    Controller::MappingError error;
    const bool loaded = mapping.load (IHUB_SET_YAML, &error);
    errorText = error.message;
    return loaded;
}

}
