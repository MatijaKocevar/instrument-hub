#include "app/DiagnosticModes.h"

#include "audio/DeviceManager.h"

#include <iostream>

namespace ihub::app {

int runListDevices(const juce::String& commandLine) {
    AudioEngine::DeviceManager deviceManager;

    AudioEngine::DeviceListOptions options;
    options.raw = commandLine.contains("--raw-devices");
    deviceManager.setDeviceListOptions(options);

    std::cout << deviceManager.describeDevices() << std::endl;
    return 0;
}

}  // namespace ihub::app
