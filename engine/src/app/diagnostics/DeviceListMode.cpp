#include "app/DiagnosticModes.h"

#include "audio/DeviceManager.h"

#include <iostream>

namespace ihub::app
{

int runListDevices()
{
    AudioEngine::DeviceManager deviceManager;
    std::cout << deviceManager.describeDevices() << std::endl;
    return 0;
}

}
