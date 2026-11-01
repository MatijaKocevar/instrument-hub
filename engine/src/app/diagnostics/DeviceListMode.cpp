#include "app/DiagnosticModes.h"

#include "audio/DeviceManager.h"

#include <iostream>

namespace ihub::app
{

// --list-devices: prints the audio device inventory.
int runListDevices()
{
    AudioEngine::DeviceManager deviceManager;
    std::cout << deviceManager.describeDevices() << std::endl;
    return 0;
}

} // namespace ihub::app
