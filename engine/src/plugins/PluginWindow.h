#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <memory>

namespace ihub::plugins
{
class PluginInstrument;

/** An application-owned window that shows a hosted plugin's native editor.

    The window creates the editor with `AudioProcessor::createEditorIfNeeded()`
    and gives ownership of it to a `juce::DocumentWindow`, so the editor is
    destroyed together with the window. Everything here is message-thread only.

    Lifetime rules:

    - Open through `open (PluginInstrument&, ...)` whenever possible. That
      installs a callback on the instrument, so calling `unload()` (or loading
      another plugin) closes the editor *before* the processor is destroyed.
      JUCE asserts if an `AudioProcessor` is destroyed while it still has an
      active editor, so this hook is what makes unload clean.
    - `close()` may be called at any time; it is idempotent.
    - A `PluginWindow` may be destroyed while a `PluginInstrument` is still
      alive: the instrument's callback only holds a weak token, so it becomes
      a no-op instead of touching freed memory.

    The window itself is a top-level `DocumentWindow` owned by this object.
    Passing `owner` centres it over that component; it does not reparent the
    plugin GUI (a third-party editor expects to be a window's content).

    @code
        ihub::plugins::PluginWindow window;
        window.open (instrument, "TestPlugin");
        // ... message loop ...
        window.close();
    @endcode

    @see PluginInstrument
*/
class PluginWindow
{
public:
    PluginWindow();
    ~PluginWindow();

    //==============================================================================
    /** Creates and shows the editor of `processor`.

        Returns false when the processor has no editor or `createEditor()`
        returned null. Any previously shown editor is closed first. Calling it
        again for the same processor brings the existing window to the front
        instead of recreating it.

        `owner` is optional and only used to position the window. When `error`
        is not null it receives a readable reason on failure (and is cleared on
        success), which is what the control API returns to the UI.
    */
    bool open (juce::AudioProcessor& processor,
               const juce::String& title = {},
               juce::Component* owner = nullptr,
               juce::String* error = nullptr);

    /** Same as above, and also arranges for `instrument.unload()` (or a
        subsequent `load()`) to close this window before the plugin instance
        is destroyed.
    */
    bool open (PluginInstrument& instrument,
               const juce::String& title = {},
               juce::Component* owner = nullptr,
               juce::String* error = nullptr);

    /** Why the most recent `open()` failed, or an empty string. */
    const juce::String& getLastError() const noexcept { return lastError; }

    /** Hides and destroys the window and its editor. Safe to call repeatedly. */
    void close();

    //==============================================================================
    bool isOpen() const noexcept             { return window != nullptr; }

    /** The editor currently shown, or null. Only valid while `isOpen()`. */
    juce::AudioProcessorEditor* getEditor() const noexcept;

    /** The `DocumentWindow` that owns the editor, or null. */
    juce::Component* getWindowComponent() const noexcept;

    /** The processor whose editor is shown, or null. */
    juce::AudioProcessor* getProcessor() const noexcept  { return processor; }

    //==============================================================================
    /** Called whenever a previously open window is closed, whether by the user,
        `close()` or `unload()`. May destroy this `PluginWindow`; nothing is
        touched afterwards. Not called from the destructor. */
    std::function<void()> onClose;

    /** Called at the start of `close()`, while the editor and its processor are
        still alive. Use it to capture the plugin's state before the editor
        stops reporting user edits. Not called from the destructor. */
    std::function<void()> onBeforeClose;

    //==============================================================================
    /** Windows currently on screen (diagnostics / leak checks). */
    static int getOpenWindowCount() noexcept;

    /** `PluginWindow` objects currently alive. Zero after the last one is
        destroyed; the debug-build JUCE leak detector is the stronger check. */
    static int getLiveInstanceCount() noexcept;

private:
    class Window;

    struct Lifetime;

    bool showEditor (juce::AudioProcessor& processor,
                     const juce::String& title,
                     juce::Component* owner);

    /** Destroys the window/editor without firing `onClose`. Used when replacing
        an editor, where the caller is still inside `this`. */
    void destroyWindow();

    /** Removes this window from the previously opened instrument, so an old
        instance's unload can never close a window that now shows a newer one. */
    void clearInstrumentCallback() noexcept;

    std::unique_ptr<Window> window;
    juce::AudioProcessor* processor = nullptr;
    PluginInstrument* instrument = nullptr;
    mutable juce::String lastError;
    std::shared_ptr<Lifetime> lifetime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

} // namespace ihub::plugins
