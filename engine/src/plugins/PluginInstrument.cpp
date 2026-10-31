#include "PluginInstrument.h"

#include <algorithm>
#include <cmath>

namespace ihub::plugins
{
namespace
{
constexpr int minimumMidiChannel = 1;
constexpr int maximumMidiChannel = 16;
constexpr int maximumMidiNote = 127;

juce::uint8 channelStatus (juce::uint8 baseStatus, int channel) noexcept
{
    const auto clamped = juce::jlimit (minimumMidiChannel, maximumMidiChannel, channel);
    return static_cast<juce::uint8> (baseStatus | (clamped - 1));
}

juce::String parameterIdentifier (juce::AudioProcessorParameter& parameter)
{
    // VST3 exposes numeric IDs; that is the only stable identifier the format
    // preserves. APVTS plugins still have readable names, checked as well.
    if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (&parameter))
        return hosted->getParameterID();

    return {};
}
} // namespace

//==============================================================================
PluginInstrument::PluginInstrument() = default;

PluginInstrument::~PluginInstrument()
{
    unload();
}

//==============================================================================
bool PluginInstrument::load (juce::AudioPluginFormatManager& formatManager,
                             const juce::PluginDescription& newDescription,
                             double initialSampleRate,
                             int initialBufferSize,
                             juce::String& error)
{
    error.clear();

    // Replacing a running instance is not allowed; the stream must be stopped.
    unload();

    sampleRate = initialSampleRate > 0.0 ? initialSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, initialBufferSize);

    auto instance = formatManager.createPluginInstance (newDescription, sampleRate, maximumBlockSize, error);

    if (instance == nullptr)
    {
        if (error.isEmpty())
            error = "createPluginInstance returned null";

        return false;
    }

    plugin = std::move (instance);
    description = newDescription;

    plugin->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);

    // Best effort: if the plugin cannot provide the requested layout, keep it
    // loaded and let process() adapt to whatever channel count it does have.
    configureBuses (numChannels);

    if (plugin->getTotalNumOutputChannels() <= 0)
    {
        error = "plugin exposes no output channels";
        unload();
        return false;
    }

    cachedLatency.store (plugin->getLatencySamples(), std::memory_order_relaxed);

    return true;
}

bool PluginInstrument::loadFromFile (juce::AudioPluginFormatManager& formatManager,
                                     const juce::File& bundle,
                                     double initialSampleRate,
                                     int initialBufferSize,
                                     juce::String& error)
{
    error.clear();

    if (! bundle.exists())
    {
        error = "plugin bundle does not exist: " + bundle.getFullPathName();
        return false;
    }

    const auto path = bundle.getFullPathName();

    juce::OwnedArray<juce::PluginDescription> types;

    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    if (types.isEmpty())
    {
        error = "no plugin types found in " + path;
        return false;
    }

    return load (formatManager, *types.getFirst(), initialSampleRate, initialBufferSize, error);
}

std::unique_ptr<PluginInstrument> PluginInstrument::create (juce::AudioPluginFormatManager& formatManager,
                                                            const juce::PluginDescription& descriptionToLoad,
                                                            double initialSampleRate,
                                                            int initialBufferSize,
                                                            juce::String& error)
{
    auto instrument = std::make_unique<PluginInstrument>();

    if (! instrument->load (formatManager, descriptionToLoad, initialSampleRate, initialBufferSize, error))
        return nullptr;

    return instrument;
}

void PluginInstrument::unload()
{
    plugin.reset();
    description = {};
    cachedLatency.store (0, std::memory_order_relaxed);
    clearPendingMidi();
    midiScratch.clear();
}

juce::String PluginInstrument::getName() const
{
    if (plugin != nullptr)
        return plugin->getName();

    return description.name;
}

//==============================================================================
bool PluginInstrument::configureBuses (int channels)
{
    if (plugin == nullptr)
        return false;

    numChannels = juce::jlimit (1, 2, channels);

    const auto target = numChannels == 1 ? juce::AudioChannelSet::mono()
                                         : juce::AudioChannelSet::stereo();

    auto layout = plugin->getBusesLayout();

    if (layout.outputBuses.isEmpty())
        return false;

    layout.outputBuses.getReference (0) = target;

    // An effect's main input follows the engine's channel count; a disabled
    // input bus stays disabled, so generators keep working.
    if (! layout.inputBuses.isEmpty())
    {
        auto& mainInput = layout.inputBuses.getReference (0);

        if (! mainInput.isDisabled())
            mainInput = target;
    }

    if (plugin->setBusesLayout (layout))
        return true;

    // Some effects only accept the output layout together with their own
    // default input layout. Retry without touching the inputs.
    auto fallback = plugin->getBusesLayout();

    if (fallback.outputBuses.isEmpty())
        return false;

    fallback.outputBuses.getReference (0) = target;

    return plugin->setBusesLayout (fallback);
}

void PluginInstrument::prepare (double newSampleRate, int newMaximumBlockSize)
{
    jassert (newSampleRate > 0.0);
    jassert (newMaximumBlockSize > 0);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);

    // Pre-allocate the merge buffer once. Normal blocks then add events
    // without touching the heap.
    midiScratch.ensureSize (midiScratchBytes);

    if (plugin == nullptr)
    {
        cachedLatency.store (0, std::memory_order_relaxed);
        return;
    }

    plugin->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
    plugin->prepareToPlay (sampleRate, maximumBlockSize);

    cachedLatency.store (plugin->getLatencySamples(), std::memory_order_relaxed);
}

void PluginInstrument::refreshLatency()
{
    cachedLatency.store (plugin != nullptr ? plugin->getLatencySamples() : 0,
                         std::memory_order_relaxed);
}

int PluginInstrument::latencySamples() const noexcept
{
    return cachedLatency.load (std::memory_order_relaxed);
}

//==============================================================================
void PluginInstrument::noteOn (int ch, int note, float vel)
{
    if (vel <= 0.0f)
    {
        noteOff (ch, note);
        return;
    }

    const auto velocity = static_cast<juce::uint8> (juce::jlimit (
        1, 127, static_cast<int> (std::lround (vel * 127.0f))));

    const juce::uint8 message[3] {
        channelStatus (0x90, ch),
        static_cast<juce::uint8> (juce::jlimit (0, maximumMidiNote, note)),
        velocity
    };

    pushEvent (message, 3);
}

void PluginInstrument::noteOff (int ch, int note)
{
    const juce::uint8 message[3] {
        channelStatus (0x80, ch),
        static_cast<juce::uint8> (juce::jlimit (0, maximumMidiNote, note)),
        0
    };

    pushEvent (message, 3);
}

bool PluginInstrument::queueMidi (const juce::uint8* bytes, int numBytes)
{
    if (bytes == nullptr || numBytes <= 0 || numBytes > 3)
        return false;

    return pushEvent (bytes, numBytes);
}

void PluginInstrument::allNotesOff (int ch)
{
    const juce::uint8 message[3] { channelStatus (0xB0, ch), 123, 0 };
    pushEvent (message, 3);
}

bool PluginInstrument::pushEvent (const juce::uint8* bytes, int numBytes) noexcept
{
    const auto write = pendingWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1) % maxPendingMidiEvents;

    if (next == pendingReadIndex.load (std::memory_order_acquire))
    {
        droppedMidiEvents.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    auto& event = pendingMidi[write];

    for (int i = 0; i < numBytes; ++i)
        event.bytes[i] = bytes[i];

    for (int i = numBytes; i < 3; ++i)
        event.bytes[i] = 0;

    event.numBytes = static_cast<juce::uint8> (numBytes);

    // Publishing the write index is what makes the event visible to process().
    pendingWriteIndex.store (next, std::memory_order_release);

    return true;
}

void PluginInstrument::drainPendingMidi (juce::MidiBuffer& destination) noexcept
{
    auto read = pendingReadIndex.load (std::memory_order_relaxed);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        const auto& event = pendingMidi[read];

        if (event.numBytes > 0)
        {
            // Notes from the control thread always belong at the start of the
            // next block; there is no per-event timestamp to preserve.
            destination.addEvent (event.bytes, event.numBytes, 0);
            deliveredMidiEvents.fetch_add (1, std::memory_order_relaxed);
        }

        read = (read + 1) % maxPendingMidiEvents;
    }

    pendingReadIndex.store (read, std::memory_order_release);
}

int PluginInstrument::getPendingMidiEventCount() const noexcept
{
    const auto read = pendingReadIndex.load (std::memory_order_acquire);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    return static_cast<int> ((write + maxPendingMidiEvents - read) % maxPendingMidiEvents);
}

int PluginInstrument::getDeliveredMidiEventCount() const noexcept
{
    return deliveredMidiEvents.load (std::memory_order_relaxed);
}

int PluginInstrument::getDroppedMidiEventCount() const noexcept
{
    return droppedMidiEvents.load (std::memory_order_relaxed);
}

void PluginInstrument::clearPendingMidi() noexcept
{
    // Only valid with the stream stopped (see the header); that is what makes
    // a plain store enough.
    pendingReadIndex.store (pendingWriteIndex.load (std::memory_order_acquire),
                            std::memory_order_release);
}

//==============================================================================
void PluginInstrument::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (plugin == nullptr)
    {
        buffer.clear();
        clearPendingMidi();
        return;
    }

    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    const auto bufferChannels = buffer.getNumChannels();
    const auto pluginChannels = juce::jmax (plugin->getTotalNumInputChannels(),
                                            plugin->getTotalNumOutputChannels());

    if (pluginChannels <= 0 || pluginChannels > bufferChannels)
    {
        // A layout the engine cannot feed. Stay silent instead of letting the
        // plugin read or write past the engine's buffer.
        buffer.clear();
        clearPendingMidi();
        return;
    }

    juce::MidiBuffer* midiToUse = &midi;

    const auto read = pendingReadIndex.load (std::memory_order_relaxed);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    if (read != write)
    {
        midiScratch.clear();
        midiScratch.addEvents (midi, 0, numSamples, 0);
        drainPendingMidi (midiScratch);
        midiToUse = &midiScratch;
    }

    if (pluginChannels == bufferChannels)
    {
        plugin->processBlock (buffer, *midiToUse);
    }
    else
    {
        // Fewer plugin channels than engine channels: hand the plugin a view.
        // The slot cleared the whole buffer first, so the remaining channels
        // stay silent.
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(),
                                       pluginChannels,
                                       numSamples);
        plugin->processBlock (view, *midiToUse);
    }
}

//==============================================================================
juce::MemoryBlock PluginInstrument::getStateInformation() const
{
    juce::MemoryBlock state;
    getStateInformation (state);
    return state;
}

void PluginInstrument::getStateInformation (juce::MemoryBlock& destination) const
{
    destination.reset();

    if (plugin != nullptr)
        plugin->getStateInformation (destination);
}

bool PluginInstrument::setStateInformation (const void* data, int numBytes)
{
    if (plugin == nullptr || data == nullptr || numBytes <= 0)
        return false;

    plugin->setStateInformation (data, numBytes);

    return true;
}

bool PluginInstrument::setStateInformation (const juce::MemoryBlock& block)
{
    return setStateInformation (block.getData(), static_cast<int> (block.getSize()));
}

//==============================================================================
int PluginInstrument::getNumParameters() const
{
    return plugin != nullptr ? plugin->getParameters().size() : 0;
}

juce::AudioProcessorParameter* PluginInstrument::findParameter (const juce::String& idOrName) const
{
    if (plugin == nullptr || idOrName.isEmpty())
        return nullptr;

    for (auto* parameter : plugin->getParameters())
    {
        if (parameter == nullptr)
            continue;

        if (parameterIdentifier (*parameter).equalsIgnoreCase (idOrName)
            || parameter->getName (64).equalsIgnoreCase (idOrName))
        {
            return parameter;
        }
    }

    return nullptr;
}

bool PluginInstrument::setParameterValue (const juce::String& idOrName, float normalisedValue)
{
    auto* parameter = findParameter (idOrName);

    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalisedValue));

    return true;
}

float PluginInstrument::getParameterValue (const juce::String& idOrName) const
{
    if (auto* parameter = findParameter (idOrName))
        return parameter->getValue();

    return -1.0f;
}

juce::String PluginInstrument::getParameterText (const juce::String& idOrName) const
{
    if (auto* parameter = findParameter (idOrName))
        return parameter->getText (parameter->getValue(), 64);

    return {};
}

} // namespace ihub::plugins
