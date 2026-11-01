#include "controller/LaunchpadDriver.h"

namespace Controller
{

namespace
{
/** Name fragment shared by the hardware and any simulator. */
constexpr const char* launchpadNameFragment = "launchpad mini mk3";

bool nameMatches (const juce::String& name)
{
    return name.toLowerCase().contains (launchpadNameFragment);
}

/** The MI port carries pad input and LED output; the DA port does not.
    Match the port suffix, not the "mi" inside "Mini". */
bool isMiPort (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    return lower.contains ("mk3 mi") || lower.endsWith ("mi")
           || lower.contains ("lpminimk3 mi");
}

bool isDawPort (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    return lower.contains ("mk3 da") || lower.endsWith ("da")
           || lower.contains ("daw");
}

const juce::MidiDeviceInfo* chooseDevice (const juce::Array<juce::MidiDeviceInfo>& devices)
{
    const juce::MidiDeviceInfo* firstMatch = nullptr;

    for (const auto& device : devices)
    {
        if (! nameMatches (device.name))
            continue;

        if (isMiPort (device.name))
            return &device;

        if (firstMatch == nullptr)
            firstMatch = &device;
    }

    // No MI port visible: accept a matching port that is not the inert DA one.
    for (const auto& device : devices)
        if (nameMatches (device.name) && ! isDawPort (device.name))
            return &device;

    return firstMatch;
}
} // namespace

//==============================================================================
// Detection
//==============================================================================

bool LaunchpadDriver::isLaunchpadName (const juce::String& deviceName)
{
    return nameMatches (deviceName);
}

std::optional<LaunchpadDriver::DeviceSelection> LaunchpadDriver::findDevice()
{
    const auto inputs = juce::MidiInput::getAvailableDevices();
    const auto outputs = juce::MidiOutput::getAvailableDevices();

    const auto* input = chooseDevice (inputs);
    const auto* output = chooseDevice (outputs);

    if (input == nullptr || output == nullptr)
        return std::nullopt;

    DeviceSelection selection;
    selection.inputIdentifier = input->identifier;
    selection.outputIdentifier = output->identifier;
    selection.inputName = input->name;
    selection.outputName = output->name;
    return selection;
}

} // namespace Controller
