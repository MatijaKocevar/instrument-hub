#pragma once

#include "../audio/FxChain.h"

// The headless module is enough for hosting, state and the audio path. The
// editor lives in PluginWindow, which is only pulled in by the .cpp, so this
// header stays usable from console tools that link the headless JUCE modules.
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <memory>

namespace juce
{
class Component;
}

namespace ihub::plugins
{
class PluginWindow;

/**
    A hosted VST3 (or any format the caller's manager knows) as an FX-chain
    element.

    `AudioEngine::FxChain` stores non-owning `juce::AudioProcessor*` entries.
    `PluginFx` owns the hosted instance, puts it into FX mode (the main input
    and output buses are both enabled and matched to the engine's channel
    count), and hands the raw processor to the chain:

    @code
        ihub::plugins::PluginFx fx;
        juce::String error;
        fx.load (formatManager, description, 48000.0, 128, error);
        chain.add (fx.getProcessor());
        chain.prepare (48000.0, 128, 2);
    @endcode

    Responsibilities:

    - **Loading / unloading.** `load()` and `loadFromFile()` instantiate the
      plugin and configure the bus layout. `unload()` first closes the editor,
      then destroys the instance. All of this is message-thread work; stop the
      audio stream before unloading or changing buses.
    - **FX mode.** Unlike `PluginInstrument`, a disabled main input bus is
      explicitly enabled. A plugin that cannot provide at least one input and
      one output channel is rejected, because it cannot process an FX send.
    - **Processing.** `process()` is the direct path for callers that do not put
      the processor in an `FxChain`; when the chain owns it, `FxChain::process()`
      calls the processor directly. Both paths are allocation-, lock- and
      log-free.
    - **Latency.** The value reported by the plugin is cached on `prepare()`
      and refreshed by `refreshLatency()`; `FxChain::refreshLatency()` sums the
      same `getLatencySamples()` value into the chain total. The plugin is never
      queried per block.
    - **State.** `getStateInformation()` / `setStateInformation()` forward to the
      plugin and give the caller a plain `juce::MemoryBlock`, which is what the
      set file stores as `fx-state/<uuid>.bin`.
    - **Editor.** `openEditor()` shows the native editor in an app-owned
      `PluginWindow`; `closeEditor()` and `unload()` close it before the
      processor is destroyed. All editor calls are message-thread only.

    @see PluginInstrument, PluginWindow, AudioEngine::FxChain
*/
class PluginFx final
{
public:
    PluginFx();
    ~PluginFx();

    //==============================================================================
    // Loading (message thread)

    /** Instantiates `description`, puts it into FX mode and caches its reported
        latency. On failure returns false, fills `error` and leaves the wrapper
        unloaded. */
    bool load (juce::AudioPluginFormatManager& formatManager,
               const juce::PluginDescription& description,
               double initialSampleRate,
               int initialBufferSize,
               juce::String& error);

    /** Instantiates the first plugin found in a bundle (`.vst3` directory or
        file) and puts it into FX mode. */
    bool loadFromFile (juce::AudioPluginFormatManager& formatManager,
                       const juce::File& bundle,
                       double initialSampleRate,
                       int initialBufferSize,
                       juce::String& error);

    /** Closes the editor, destroys the hosted instance and clears cached
        latency. Remove it from any `FxChain` first (the chain holds a
        non-owning pointer) and stop the audio stream. */
    void unload();

    bool isLoaded() const noexcept                    { return plugin != nullptr; }

    //==============================================================================
    // Bus layout and preparation (message thread)

    /** Enables the main input and output buses and matches them to
        `numChannels` (1 = mono, 2 = stereo). Returns false when the plugin
        cannot provide an FX layout with at least one input and one output. */
    bool configureBuses (int numChannels = 2);

    /** True when the loaded plugin has an active main input and output bus. */
    bool isFxMode() const noexcept;

    /** Engine channel count requested from `configureBuses()`. */
    int getNumChannels() const noexcept               { return numChannels; }

    int getNumInputChannels() const noexcept;
    int getNumOutputChannels() const noexcept;

    /** Forwards rate/block size to the plugin, prepares it and caches the
        latency it reports. Safe to call again after a device change or after
        restoring state. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Releases the plugin's audio resources; keeps the instance loaded. The
        stream must be stopped. */
    void releaseResources();

    /** Re-reads the plugin's latency. Call off the audio thread after changing
        a parameter that alters it. */
    void refreshLatency() noexcept;

    /** Latency cached by `prepare()` / `refreshLatency()`, in samples. Safe
        from any thread; DECISIONS says not to query the plugin per block. */
    int latencySamples() const noexcept;

    //==============================================================================
    // FX chain integration (message thread)

    /** The processor to pass to `AudioEngine::FxChain::add()`.

        The wrapper keeps ownership: the pointer stays valid until `unload()`
        or destruction, so remove it from the chain first.
    */
    juce::AudioProcessor* getProcessor() const noexcept;

    /** The hosted instance, or null. */
    juce::AudioPluginInstance* getPlugin() const noexcept   { return plugin.get(); }

    /** Appends the hosted processor to `chain`; returns its index or -1. */
    int addToChain (AudioEngine::FxChain& chain);

    /** Removes the hosted processor from `chain`; false when it is not there. */
    bool removeFromChain (AudioEngine::FxChain& chain) noexcept;

    bool isInChain (const AudioEngine::FxChain& chain) const noexcept;

    //==============================================================================
    // Audio thread

    /** Renders one block through the hosted plugin.

        For callers that process `PluginFx` directly. When the processor is in
        an `FxChain`, the chain calls the plugin instead; this method exists so
        a single effect can be driven without building a chain. A plugin with
        fewer channels than the engine gets a view of the first channels, and
        an unloaded wrapper clears the buffer.

        No allocation, locks, logging or file I/O.
    */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    //==============================================================================
    // State (message thread)

    /** The plugin's opaque state, ready to be written to a `.bin` chunk. */
    juce::MemoryBlock getStateInformation() const;

    /** Same, writing into an existing block. */
    void getStateInformation (juce::MemoryBlock& destination) const;

    bool setStateInformation (const void* data, int numBytes);
    bool setStateInformation (const juce::MemoryBlock& block);

    //==============================================================================
    // Parameters (message thread)

    int getNumParameters() const;

    /** Finds a parameter by `HostedAudioProcessorParameter::getParameterID()`
        or by its display name (case-insensitive). */
    juce::AudioProcessorParameter* findParameter (const juce::String& idOrName) const;

    /** Sets a normalised (0..1) value, notifying the host. */
    bool setParameterValue (const juce::String& idOrName, float normalisedValue);

    /** Reads a normalised (0..1) value, or -1 when the parameter is missing. */
    float getParameterValue (const juce::String& idOrName) const;

    /** Human-readable value, e.g. "5.0 ms". Empty when missing. */
    juce::String getParameterText (const juce::String& idOrName) const;

    //==============================================================================
    // Editor (message thread)

    /** Creates and shows the native editor in an app-owned window.

        Returns false when nothing is loaded or the plugin has no editor. Any
        previously shown editor is closed first. `owner` is optional and only
        positions the window.
    */
    bool openEditor (const juce::String& title = {},
                     juce::Component* owner = nullptr);

    /** Hides and destroys the editor window. Safe to call repeatedly. */
    void closeEditor();

    bool isEditorOpen() const noexcept;
    juce::AudioProcessorEditor* getEditor() const noexcept;
    juce::Component* getEditorWindow() const noexcept;

    //==============================================================================
    juce::String getName() const;

    /** Description of the loaded type; only valid while loaded. */
    const juce::PluginDescription& getPluginDescription() const noexcept
    {
        return description;
    }

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    int numChannels = 2;

    std::atomic<int> cachedLatency { 0 };

    std::unique_ptr<PluginWindow> editorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginFx)
};

} // namespace ihub::plugins
