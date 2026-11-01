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

/**
    Builds and opens the composed audio session.

    The graph is created once: an AudioEngine with eight fixed slots, a
    SlotSwitcher over them, and the default set created through the existing
    instrument factories. The configured device is then opened and the session
    becomes its callback. Nothing is rewired at runtime; arming a slot only
    publishes a lock-free crossfade request.
*/
std::unique_ptr<ihub::Session> startSession (const juce::String& commandLine)
{
    if (commandLine.contains ("--no-audio"))
    {
        std::cout << "session: audio disabled by --no-audio" << std::endl;
        return {};
    }

    ihub::SessionConfig config;
    config.slots = ihub::Session::makeDefaultSet();

    // Give every pad its real family/patch identity from the shipped set
    // file. The audio sources stay initialisation tones until a set with
    // sample kits or plugin bundles is loaded, but `/state` already mirrors
    // the 8 x 8 tree the Launchpad lights up.
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

                // Persisted mix/colour/descriptor (T119). The audio source
                // stays an init tone until a real kit or plugin is loaded,
                // but the API publishes the set's full patch tree.
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

                // A patch with no assigned source is silent. Only a
                // descriptor that names a usable source becomes audible;
                // a missing kit or bundle falls back to `empty` rather
                // than a placeholder tone.
                applySourceKind (juce::String (instrument->kind), slotConfig);

                // Persisted insert chain (T119/T121). A built-in's id is
                // its type; a plugin entry carries the bundle path in id.
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

    // Load every patch's own plugin state before `build()` starts warming
    // the initial slot, so the first resident instance is already restored
    // (T130).
    if (! setFile.empty())
        session->setPluginStateDirectory (pluginStateDirectoryFor (setFile));

    juce::String error;

    if (! session->build (config, error))
    {
        std::cerr << "session: could not build the default set: " << error << std::endl;
        return {};
    }

    // The Instrument page's source picker persists its assignment in a
    // sidecar next to set.yaml; a restart re-applies it before audio starts.
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

    // The default set is playable immediately. The control API mirrors it
    // and routes arm requests into the switcher once the state store is
    // created below.
    session->armSlot (0);

    return session;
}

} // namespace ihub::app
