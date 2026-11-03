#include "DeviceTypes.h"

#include "audio/DeviceManager.h"

#include <iostream>
#include <string>

namespace
{

int check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;
    return condition ? 0 : 1;
}

}

int DeviceManagerTest::checkDeviceTypes (AudioEngine::DeviceManager& manager)
{
    int failures = 0;

    const auto typeNames = manager.getDeviceTypeNames();
    std::cout << "device types: " << typeNames.joinIntoString (", ") << std::endl;

    failures += check (typeNames.contains ("ALSA"), "ALSA is an audio device type");
    failures += check (typeNames.contains ("JACK"), "JACK is an audio device type");

    bool foundJack = false;
    bool jackHasOutput = false;

    for (const auto& type : manager.getDeviceTypes())
        if (type.name == "JACK")
        {
            foundJack = true;
            jackHasOutput = ! type.outputDeviceNames.isEmpty();
        }

    failures += check (foundJack, "JACK reports a device type listing");
    failures += check (foundJack && ! jackHasOutput,
                       "JACK reports no output devices when no server is running");

    bool jackDeviceListed = false;

    for (const auto& device : manager.getDevices())
        if (device.typeName == "JACK")
            jackDeviceListed = true;

    failures += check (! jackDeviceListed,
                       "the device list holds no JACK entry when no server is running");

    return failures;
}
