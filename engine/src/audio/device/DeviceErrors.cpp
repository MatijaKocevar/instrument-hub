#include "DeviceErrors.h"

namespace AudioEngine::detail
{

juce::String describeDeviceError (const juce::String& error)
{
    if (error.isEmpty())
        return error;

    if (error.containsIgnoreCase ("busy")
        || error.containsIgnoreCase ("in use"))
    {
        return "the device is in use by another process (PipeWire)";
    }

    if (error.containsIgnoreCase ("no such device")
        || error.containsIgnoreCase ("not available")
        || error.containsIgnoreCase ("not connected"))
    {
        return "the device is not connected";
    }

    return error;
}

} // namespace AudioEngine::detail
