#include "audio/DeviceManager.h"

#include "audio/device/DevicePcmHints.h"

#include <utility>

namespace AudioEngine
{

std::vector<DeviceInfo> DeviceManager::getRawPcmDevices()
{
    std::vector<DeviceInfo> result;
    auto* type = findDeviceType ("ALSA");

    if (type == nullptr)
        return result;

    const auto hints = detail::readAlsaPcmHints();

    for (const auto& name : type->getDeviceNames (false))
    {
        DeviceInfo info;
        info.typeName = "ALSA";
        info.name = name;
        info.description = detail::pcmNameForJuceDescription (hints, name);
        info.juceName = name;
        info.hasOutput = true;
        result.push_back (std::move (info));
    }

    return result;
}

} // namespace AudioEngine
