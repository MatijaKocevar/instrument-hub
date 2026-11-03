#include "SessionSetup.h"

#include "CommandLine.h"
#include "ProjectSetup.h"

#include "Session.h"

#include <iostream>
#include <utility>

namespace ihub::app
{

std::unique_ptr<ihub::Session> startSession (const juce::String& commandLine,
                                             const juce::File& projectFile)
{
    juce::String projectError;
    bool imported = false;
    const bool projectAvailable = ensureProjectFile (projectFile, projectError, &imported);

    if (! projectAvailable)
        std::cerr << "project: " << projectError << std::endl;
    else if (imported)
        std::cout << "project: imported the bundled set into "
                  << projectFile.getFullPathName() << std::endl;

    ihub::SessionConfig config;
    config.slots = ihub::Session::makeDefaultSet();

    if (projectAvailable)
    {
        if (! loadProjectSession (projectFile, config, projectError))
        {
            std::cerr << "project: " << projectError << std::endl;
            config.slots = ihub::Session::makeDefaultSet();
        }
        else
        {
            std::cout << "project: loaded " << projectFile.getFullPathName()
                      << " (" << config.slots.size() << " slots)" << std::endl;
        }
    }

    if (commandLine.contains ("--no-audio"))
    {
        std::cout << "session: audio disabled by --no-audio" << std::endl;
        return {};
    }

    if (const auto type = getCommandLineValue (commandLine, "--audio-type"); type.isNotEmpty())
        config.device.deviceTypeName = type;

    if (const auto name = getCommandLineValue (commandLine, "--audio-device"); name.isNotEmpty())
        config.device.outputDeviceName = name;

    if (const auto rate = getCommandLineValue (commandLine, "--audio-sample-rate"); rate.isNotEmpty())
        config.device.sampleRate = rate.getDoubleValue();

    if (const auto block = getCommandLineValue (commandLine, "--audio-buffer-size"); block.isNotEmpty())
        config.device.bufferSize = block.getIntValue();

    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;

    auto session = std::make_unique<ihub::Session>();

    juce::String error;

    if (! session->build (config, error))
    {
        std::cerr << "session: could not build the set: " << error << std::endl;
        return {};
    }

    if (! session->openDevice (&error))
    {
        std::cerr << "session: audio is not running: " << error << std::endl;
    }
    else
    {
        std::cout << "session: opened " << session->getDevices().getCurrentDeviceName()
                  << " at " << session->getDevices().getCurrentSampleRate()
                  << " Hz / " << session->getDevices().getCurrentBufferSize()
                  << " frames, " << session->getNumSlots()
                  << " slots, project loaded" << std::endl;
    }

    session->armSlot (0);

    return session;
}

}
