#pragma once

#include "../audio/FxChain.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <memory>

namespace juce {
class Component;
}

namespace ihub::plugins {
class PluginWindow;

class PluginFx final {
public:
    PluginFx();
    ~PluginFx();

    bool load(juce::AudioPluginFormatManager& formatManager,
              const juce::PluginDescription& description, double initialSampleRate,
              int initialBufferSize, juce::String& error);

    bool loadFromFile(juce::AudioPluginFormatManager& formatManager, const juce::File& bundle,
                      double initialSampleRate, int initialBufferSize, juce::String& error);

    void unload();

    bool isLoaded() const noexcept { return plugin != nullptr; }

    bool configureBuses(int numChannels = 2);

    bool isFxMode() const noexcept;

    int getNumChannels() const noexcept { return numChannels; }

    int getNumInputChannels() const noexcept;
    int getNumOutputChannels() const noexcept;

    void prepare(double sampleRate, int maximumBlockSize);

    void releaseResources();

    void refreshLatency() noexcept;

    int latencySamples() const noexcept;

    juce::AudioProcessor* getProcessor() const noexcept;

    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    int addToChain(AudioEngine::FxChain& chain);

    bool removeFromChain(AudioEngine::FxChain& chain) noexcept;

    bool isInChain(const AudioEngine::FxChain& chain) const noexcept;

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    juce::MemoryBlock getStateInformation() const;

    void getStateInformation(juce::MemoryBlock& destination) const;

    bool setStateInformation(const void* data, int numBytes);
    bool setStateInformation(const juce::MemoryBlock& block);

    int getNumParameters() const;

    juce::AudioProcessorParameter* findParameter(const juce::String& idOrName) const;

    bool setParameterValue(const juce::String& idOrName, float normalisedValue);

    float getParameterValue(const juce::String& idOrName) const;

    juce::String getParameterText(const juce::String& idOrName) const;

    bool openEditor(const juce::String& title = {}, juce::Component* owner = nullptr);

    void closeEditor();

    bool isEditorOpen() const noexcept;
    juce::AudioProcessorEditor* getEditor() const noexcept;
    juce::Component* getEditorWindow() const noexcept;

    juce::String getName() const;

    const juce::PluginDescription& getPluginDescription() const noexcept { return description; }

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    int numChannels = 2;

    std::atomic<int> cachedLatency{0};

    std::unique_ptr<PluginWindow> editorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFx)
};

}  // namespace ihub::plugins
