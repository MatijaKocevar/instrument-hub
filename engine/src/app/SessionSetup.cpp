#include "SessionSetup.h"

#include "CommandLine.h"
#include "SourcePersistence.h"

#include "Session.h"
#include "controller/Mapping.h"

#include <cmath>
#include <iostream>
#include <utility>

namespace ihub::app
{

std::unique_ptr<ihub::Session> startSession (const juce::String& commandLine)
{
    if (commandLine.contains ("--no-audio"))
    {
        std::cout << "session: audio disabled by --no-audio" << std::endl;
        return {};
    }

    ihub::SessionConfig config;
    config.slots = ihub::Session::makeDefaultSet();

    const auto setFile = Controller::Mapping::findDefaultSetFile();

    if (! setFile.empty())
    {
        Controller::SetMapping document;
        Controller::MappingError mappingError;

        if (Controller::loadSetMappingFile (setFile, document, mappingError))
        {
            for (int pad = 0; pad < Controller::SetMapping::maximumPads; ++pad)
            {
                const auto* binding = document.padBinding (pad);
                const auto* instrument = binding != nullptr
                                             ? document.instrumentFor (*binding)
                                             : nullptr;

                if (binding == nullptr || instrument == nullptr)
                    continue;

                const int slot = instrument->slot >= 0 ? instrument->slot
                                                       : binding->slot;

                if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
                    continue;

                auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
                slotConfig.name = instrument->name;
                slotConfig.patch = instrument->id;
                slotConfig.patchName = instrument->name;
                slotConfig.family = binding->family;
                slotConfig.familyName = binding->family;

                slotConfig.gain = instrument->volumeDb <= -60.0f
                                      ? 0.0f
                                      : std::pow (10.0f, instrument->volumeDb / 20.0f);
                slotConfig.pan = instrument->pan;
                slotConfig.muted = instrument->muted;
                slotConfig.solo = instrument->solo;
                slotConfig.colorIndex = instrument->resolvedColorIndex;
                slotConfig.instrumentKind = instrument->kind;
                slotConfig.pluginFormat = instrument->pluginFormat;
                slotConfig.pluginId = instrument->pluginId;
                slotConfig.pluginName = instrument->pluginName;
                slotConfig.pluginManufacturer = instrument->pluginManufacturer;
                slotConfig.pluginVersion = instrument->pluginVersion;
                slotConfig.samplePath = instrument->samplePath;

                applySourceKind (juce::String (instrument->kind), slotConfig);

                for (const auto& fxSpec : instrument->fxChain)
                {
                    AudioEngine::FxInsertConfig fx;
                    fx.bypassed = fxSpec.bypassed;
                    fx.id = fxSpec.id;
                    fx.name = fxSpec.name;

                    if (fxSpec.kind == "plugin")
                    {
                        fx.type = AudioEngine::FxInsertType::vst3;
                        fx.path = fxSpec.id;
                    }
                    else if (! AudioEngine::fxInsertTypeFromString (fxSpec.id, fx.type))
                    {
                        continue;
                    }

                    for (const auto& [key, value] : fxSpec.parameters)
                    {
                        if (key == "wet")
                            fx.wet = juce::jlimit (0.0f, 1.0f, value);
                        else
                            fx.parameters[key] = value;
                    }

                    slotConfig.fxChain.push_back (std::move (fx));
                }

                if (const auto family = document.families.find (binding->family);
                    family != document.families.end())
                {
                    slotConfig.familyName = family->second.name;
                }
            }
        }
        else
        {
            std::cerr << "session: could not read " << setFile.string()
                      << ": " << mappingError.message << std::endl;
        }
    }

    if (! setFile.empty())
        loadPersistedSources (config, setFile);

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

    if (! setFile.empty())
        session->setPluginStateDirectory (pluginStateDirectoryFor (setFile));

    juce::String error;

    if (! session->build (config, error))
    {
        std::cerr << "session: could not build the default set: " << error << std::endl;
        return {};
    }

    if (! setFile.empty())
    {
        session->setSourceAssignedHook ([setFile] (int slot,
                                                   const ihub::SessionSlotConfig& source)
        {
            persistSourceAssignment (setFile, slot, source);
        });
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
                  << " slots, default set loaded" << std::endl;
    }

    session->armSlot (0);

    return session;
}

}
