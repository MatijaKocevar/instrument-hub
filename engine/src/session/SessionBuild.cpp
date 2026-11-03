#include "Session.h"

#include "instruments/AudioInputInstrument.h"
#include "plugins/PluginInstrument.h"
#include "plugins/SandboxHost.h"
#include "session/EditorHost.h"
#include "session/SlotSources.h"

#if defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) && JUCE_MODULE_AVAILABLE_juce_gui_basics
 #include "plugins/PluginWindow.h"
#endif

#include <algorithm>
#include <memory>

namespace ihub
{

namespace
{
constexpr float initToneAmplitude = 0.12f;
}

bool Session::build (const SessionConfig& newConfig, juce::String& error)
{
    error.clear();
    closeDevice();
    prepared = false;
    transportControl.reset();

    if (editorHost != nullptr)
        editorHost->closeAll();

    releaseAllPluginMirrors();

    cancelPendingLoads();
    warmPool.shutdown();
    shuttingDown.store (false, std::memory_order_release);
    loadAbort = std::make_shared<std::atomic<bool>> (false);
    switcher.reset();
    engine.reset();
    fxChain.reset();
    warmPool.clearVoices();
    pluginFormats.reset();

    {
        const std::lock_guard<std::mutex> lock (slotErrorMutex);
        slotErrors.assign (defaultLogicalSlots, {});
    }

    config = newConfig;

    if (config.slots.empty())
        config.slots = makeDefaultSet();

    if (config.device.sampleRate <= 0.0)
        config.device.sampleRate = 48000.0;

    if (config.device.bufferSize <= 0)
        config.device.bufferSize = 128;

    if (config.device.numOutputChannels <= 0)
        config.device.numOutputChannels = 2;

    const int slotCount = static_cast<int> (config.slots.size());

    if (slotCount <= 0)
    {
        error = "the session description has no slots";
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (sourceMutex);
        sourceConfigs = config.slots;
        sourceRevision = 0;
    }

    if (slotCount > defaultLogicalSlots)
    {
        error = "the session description has " + juce::String (slotCount)
              + " slots; the Launchpad grid has " + juce::String (defaultLogicalSlots);
        return false;
    }

    for (int index = 0; index < slotCount; ++index)
    {
        const auto& slotConfig = config.slots[static_cast<std::size_t> (index)];

        if (slotConfig.kind != SessionSlotKind::plugin
            || slotConfig.pluginState.getSize() == 0)
            continue;

        const session::PluginStateIdentity identity {
            juce::String (slotConfig.path),
            juce::String (slotConfig.pluginId),
            juce::String (slotConfig.pluginName)
        };

        (void) pluginState.seedState (index, identity, slotConfig.pluginState);
    }

    engine = std::make_unique<AudioEngine::AudioEngine> (slotCount,
                                                         config.device.numOutputChannels);
    switcher = std::make_unique<AudioEngine::SlotSwitcher> (config.fadeMilliseconds);

    pluginFormats = std::make_unique<juce::AudioPluginFormatManager>();

   #if defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) && JUCE_MODULE_AVAILABLE_juce_gui_basics
    juce::addDefaultFormatsToManager (*pluginFormats);
   #else
    juce::addHeadlessDefaultFormatsToManager (*pluginFormats);
   #endif

    const int neighbours = juce::jmax (1, config.neighbourCount);

    warmPool.createPool (neighbours);
    warmPool.createVoices (slotCount);

    warmPool.configure (
        [this] (int slotIndex, juce::String& factoryError)
        {
            if (config.loadDelayMilliseconds > 0)
                juce::Thread::sleep (config.loadDelayMilliseconds);

            return createPooledSource (slotIndex, factoryError);
        },
        [this] (AudioEngine::SlotSource& instance)
        {
            if (auto* sandbox = dynamic_cast<plugins::SandboxHost*> (&instance);
                sandbox != nullptr)
            {
                unregisterSandboxSlot (*sandbox);
            }
            else if (auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);
                     instrument != nullptr)
            {
                capturePluginStateForInstance (instance);
                pluginState.unregisterInstrument (*instrument);
            }

            if (editorHost != nullptr)
                editorHost->closeBeforeDestroy (instance);
        },
        [this] (int slot)
        {
            session::PluginStateIdentity identity;

            if (pluginIdentityForSlot (slot, identity))
                pluginState.applyToRegistered (slot, identity);
        },
        switcher.get(),
        [this] { cancelPendingLoads(); });

    bool anySolo = false;

    for (const auto& slotConfig : config.slots)
        anySolo = anySolo || slotConfig.solo;

    std::vector<AudioEngine::Slot*> slotPointers;
    slotPointers.reserve (static_cast<std::size_t> (slotCount));

    for (int index = 0; index < slotCount; ++index)
    {
        const auto& slotConfig = config.slots[static_cast<std::size_t> (index)];
        const bool silent = slotConfig.muted || (anySolo && ! slotConfig.solo);

        auto& slot = engine->getSlot (index);
        slot.setSource (warmPool.getVoiceSource (index));
        slot.setGain (silent ? 0.0f : slotConfig.gain);

        slotPointers.push_back (&slot);
    }

    switcher->setSlots (slotPointers);

    warmPool.setFormat (config.device.sampleRate, config.device.bufferSize);
    warmPool.setNumSlots (slotCount);

    for (int index = 0; index < slotCount; ++index)
    {
        const auto& slotConfig = config.slots[static_cast<std::size_t> (index)];

        warmPool.setRequest (index,
                             juce::String (index) + ":" + toString (slotConfig.kind)
                                 + ":" + slotConfig.path + ":"
                                 + juce::String (slotConfig.frequency));
    }

    prepare (config.device.sampleRate,
             config.device.bufferSize,
             config.device.numOutputChannels);

    fxChain.setEngine (engine.get());
    fxChain.setFormat (sampleRate, maximumBlockSize, numChannels);
    fxChain.build (config.slots, *pluginFormats, transportControl.getFxTransport());

    const int initial = juce::jlimit (0, slotCount - 1, config.initialSlot);
    warmPool.setActiveSlot (initial);
    warmPool.preloadAll();
    warmPool.resetArmState();

    return true;
}

std::unique_ptr<AudioEngine::SlotSource> Session::createPooledSource (int slotIndex,
                                                                      juce::String& error)
{
    SessionSlotConfig slotConfig;

    {
        const std::lock_guard<std::mutex> lock (sourceMutex);

        if (slotIndex < 0 || slotIndex >= static_cast<int> (sourceConfigs.size()))
        {
            error = "warm request addresses slot " + juce::String (slotIndex);
            return nullptr;
        }

        slotConfig = sourceConfigs[static_cast<std::size_t> (slotIndex)];
    }

    const auto slotLabel = juce::String (slotIndex) + " (\"" + juce::String (slotConfig.name) + "\")";

    switch (slotConfig.kind)
    {
        case SessionSlotKind::empty:
            return session::makeSilentSource();

        case SessionSlotKind::initTone:
            return session::makeInitToneSource (slotConfig.frequency, initToneAmplitude);

        case SessionSlotKind::sampler:
        {
            if (slotConfig.path.isEmpty())
            {
                error = "slot " + slotLabel + " is a sampler slot but has no kit path";
                return nullptr;
            }

            auto samplerSource = session::makeSamplerSource (juce::File (slotConfig.path),
                                                             config.device.sampleRate,
                                                             error);

            if (samplerSource == nullptr)
            {
                error = "slot " + slotLabel + ": " + error;
                return nullptr;
            }

            return samplerSource;
        }

        case SessionSlotKind::plugin:
        {
            if (slotConfig.path.isEmpty())
            {
                error = "slot " + slotLabel + " is a plugin slot but has no bundle path";
                return nullptr;
            }

            plugins::SandboxHost::Options options;
            options.pluginBundle = juce::File (slotConfig.path);
            options.sampleRate = config.device.sampleRate;
            options.maxBlockSize = config.device.bufferSize;
            options.numChannels = juce::jlimit (1, 2, config.device.numOutputChannels);

            if (loadAbort != nullptr)
            {
                const auto abort = loadAbort;
                options.shouldAbortStart = [abort]
                {
                    return abort->load (std::memory_order_relaxed);
                };
            }

            auto instrument = std::make_unique<plugins::SandboxHost> (options);

            instrument->setStatusCallback (
                [this, slotIndex] (plugins::SandboxHost::Status,
                                   const juce::String& reason)
                {
                    setSlotError (slotIndex, reason);
                    juce::Logger::writeToLog ("session: slot " + juce::String (slotIndex)
                                              + " plugin failed: " + reason);
                });

            if (! instrument->start (error))
            {
                error = "slot " + slotLabel + ": " + error;
                setSlotError (slotIndex, error);
                return nullptr;
            }

            const session::PluginStateIdentity identity {
                juce::String (slotConfig.path),
                juce::String (slotConfig.pluginId),
                juce::String (slotConfig.pluginName)
            };

            const auto storedState = pluginState.readState (slotIndex, identity);

            if (storedState.getSize() > 0
                && ! instrument->setStateInformation (storedState))
            {
                juce::Logger::writeToLog ("session: slot " + juce::String (slotIndex)
                                          + " rejected stored plugin state");
            }

            registerSandboxSlot (slotIndex, *instrument, identity);
            clearSlotError (slotIndex);

            return instrument;
        }

        case SessionSlotKind::audioInput:
        {
            auto instrument = std::make_unique<AudioInputInstrument>();
            instrument->setInputChannel (slotConfig.inputChannel);
            return instrument;
        }
    }

    error = "slot " + slotLabel + " has an unknown source kind";
    return nullptr;
}

void Session::prepare (double newSampleRate, int newMaximumBlockSize, int newNumChannels)
{
    if (engine == nullptr)
    {
        prepared = false;
        return;
    }

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    numChannels = juce::jmax (1, newNumChannels);

    warmPool.setFormat (sampleRate, maximumBlockSize);

    engine->prepare (sampleRate, maximumBlockSize, numChannels);

    if (switcher != nullptr)
        switcher->prepare (sampleRate, maximumBlockSize, numChannels);

    transportControl.prepare (sampleRate, maximumBlockSize);

    meters.prepare (juce::jmax (1, engine->getNumSlots()));

    renderBuffer.setSize (numChannels, maximumBlockSize, false, true, true);
    renderBuffer.clear();

    midi.prepare();

    prepared = true;
}

void Session::resetGraph() noexcept
{
    if (engine != nullptr)
        engine->reset();

    if (switcher != nullptr)
        switcher->reset();

    transportControl.rewind();

    meters.reset();

    renderBuffer.clear();
    midi.reset();
}

}
