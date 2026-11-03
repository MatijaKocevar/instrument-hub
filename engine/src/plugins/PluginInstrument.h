#pragma once

#include "../instruments/Instrument.h"

#include "../audio/Slot.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace ihub::plugins
{

class PluginInstrument final : public Instrument,
                               public AudioEngine::SlotSource
{
public:
    static constexpr int maxPendingMidiEvents = 256;

    static constexpr size_t midiScratchBytes = 16384;

    PluginInstrument();
    ~PluginInstrument() override;

    bool load (juce::AudioPluginFormatManager& formatManager,
               const juce::PluginDescription& description,
               double initialSampleRate,
               int initialBufferSize,
               juce::String& error);

    bool loadFromFile (juce::AudioPluginFormatManager& formatManager,
                       const juce::File& bundle,
                       double initialSampleRate,
                       int initialBufferSize,
                       juce::String& error);

    static std::unique_ptr<PluginInstrument> create (juce::AudioPluginFormatManager& formatManager,
                                                     const juce::PluginDescription& description,
                                                     double initialSampleRate,
                                                     int initialBufferSize,
                                                     juce::String& error);

    void unload();

    void setBeforeUnloadCallback (std::function<void()> callback);

    using ParameterChangedCallback = std::function<void (const juce::String& parameterId,
                                                         float normalisedValue)>;

    void setParameterChangedCallback (ParameterChangedCallback callback);

    void setStateChangedCallback (std::function<void()> callback);

    bool isLoaded() const noexcept { return plugin != nullptr; }

    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    const juce::PluginDescription& getPluginDescription() const noexcept { return description; }

    juce::String getName() const;

    bool configureBuses (int numChannels = 2);

    int getNumChannels() const noexcept { return numChannels; }

    void prepare (double sampleRate, int maxBlock) override;

    void refreshLatency();

    int latencySamples() const noexcept override;

    Instrument::Kind kind() const noexcept override { return Instrument::Kind::Plugin; }

    void noteOn (int ch, int note, float vel) override;
    void noteOff (int ch, int note) override;

    bool queueMidi (const juce::uint8* bytes, int numBytes);

    void allNotesOff (int ch = 1);

    int getPendingMidiEventCount() const noexcept;

    int getDeliveredMidiEventCount() const noexcept;

    int getDroppedMidiEventCount() const noexcept;

    void clearPendingMidi() noexcept;

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::MemoryBlock getStateInformation() const;

    void getStateInformation (juce::MemoryBlock& destination) const;

    bool setStateInformation (const void* data, int numBytes);
    bool setStateInformation (const juce::MemoryBlock& block);

    int getNumParameters() const;

    juce::String getParameterId (int index) const;

    juce::AudioProcessorParameter* findParameter (const juce::String& idOrName) const;

    bool setParameterValue (const juce::String& idOrName, float normalisedValue);

    bool setParameterValueByIndex (int index, float normalisedValue);

    float getParameterValue (const juce::String& idOrName) const;

    juce::String getParameterText (const juce::String& idOrName) const;

private:
    struct PendingMidiEvent
    {
        std::uint8_t bytes[3] { 0, 0, 0 };
        std::uint8_t numBytes = 0;
    };

    bool pushEvent (const juce::uint8* bytes, int numBytes) noexcept;
    void drainPendingMidi (juce::MidiBuffer& destination) noexcept;
    void notifyParameterChanged (int parameterIndex, float normalisedValue);
    void notifyStateChanged();
    void refreshListener();

    class StateChangeForwarder;

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    int numChannels = 2;

    std::atomic<int> cachedLatency { 0 };

    std::array<PendingMidiEvent, maxPendingMidiEvents> pendingMidi {};
    std::atomic<std::uint32_t> pendingWriteIndex { 0 };
    std::atomic<std::uint32_t> pendingReadIndex { 0 };
    std::atomic<int> deliveredMidiEvents { 0 };
    std::atomic<int> droppedMidiEvents { 0 };

    juce::MidiBuffer midiScratch;

    std::function<void()> beforeUnloadCallback;
    ParameterChangedCallback parameterChangedCallback;
    std::function<void()> stateChangedCallback;
    std::unique_ptr<StateChangeForwarder> stateChangeForwarder;
    mutable std::mutex stateMutex;
    mutable std::atomic<bool> suppressNotifications { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginInstrument)
};

}
