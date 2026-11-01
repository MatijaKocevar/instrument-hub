#include "Session.h"

#include "instruments/AudioInputInstrument.h"
#include "plugins/PluginInstrument.h"
#include "session/EditorHost.h"
#include "session/SlotSources.h"

#if defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) && JUCE_MODULE_AVAILABLE_juce_gui_basics
 // The GUI build hosts the standard VST3 format, whose declaration lives in
 // the non-headless module.
 #include "plugins/PluginWindow.h"
#endif

#include <algorithm>
#include <memory>

namespace ihub
{

namespace
{
/** Amplitude of the built-in initialisation tone. */
constexpr float initToneAmplitude = 0.12f;
} // namespace

bool Session::build (const SessionConfig& newConfig, juce::String& error)
{
    error.clear();
    closeDevice();
    prepared = false;
    transportControl.reset();

    // Editors reference the old instances; close them before the graph and the
    // warm pool are dropped.
    if (editorHost != nullptr)
        editorHost->closeAll();

    // Drop the graph before its proxies: the slots hold non-owning pointers.
    warmPool.shutdown();
    switcher.reset();
    engine.reset();
    fxChain.reset();
    warmPool.clearVoices();
    pluginFormats.reset();

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
        // The pool factory reads this copy, so a later source swap can never
        // race a load that is already in flight.
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

    engine = std::make_unique<AudioEngine::AudioEngine> (slotCount,
                                                         config.device.numOutputChannels);
    switcher = std::make_unique<AudioEngine::SlotSwitcher> (config.fadeMilliseconds);

    pluginFormats = std::make_unique<juce::AudioPluginFormatManager>();

   #if defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) && JUCE_MODULE_AVAILABLE_juce_gui_basics
    // The GUI build hosts the standard VST3 format, whose instances expose
    // their native editor. The headless format would return hasEditor() ==
    // false even for a plugin that has a window.
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
            // Capture before the processor is destroyed. The pool may evict a
            // patch at any time, so this is the last reliable point to store
            // its state (T130). The editor host then closes the window while
            // the processor is still alive.
            if (auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);
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
        switcher.get());

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

    // All slots are wired in parallel once. Nothing rewires later; arming only
    // changes which warm-pool instance the fixed proxy points at.
    switcher->setSlots (slotPointers);

    // Give the pool its format before any request can be queued. `build()`
    // calls `setRequest()` before `prepare()`, and a pool without a format
    // would create an unprepared instrument (silent init tone).
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

    // The rack owns the insert processors; the slot's FxChain only holds
    // non-owning pointers, so build the racks after the graph is prepared and
    // rebuild each chain once from the persisted set.
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

            auto instrument = std::make_unique<plugins::PluginInstrument>();

            if (! instrument->loadFromFile (*pluginFormats,
                                            juce::File (slotConfig.path),
                                            config.device.sampleRate,
                                            config.device.bufferSize,
                                            error))
            {
                error = "slot " + slotLabel + ": " + error;
                return nullptr;
            }

            // Restore this patch's own state before the instance is published
            // to a slot. The warm pool prepares it afterwards, exactly as if a
            // fresh preset had been chosen in the plugin.
            const session::PluginStateIdentity identity {
                juce::String (slotConfig.path),
                juce::String (slotConfig.pluginId),
                juce::String (slotConfig.pluginName)
            };
            pluginState.apply (slotIndex, *instrument, identity, true);

            // The editor host resolves this slot to this instance. An older
            // instance stays registered until its own destroy hook forgets it.
            if (editorHost != nullptr)
                editorHost->registerInstrument (slotIndex, instrument.get());

            pluginState.registerInstrument (slotIndex, *instrument);

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

    // The engine owns the slots, the summing bus and the timing monitor. The
    // switcher owns the crossfade envelope over the same slots.
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

} // namespace ihub
