#include "PluginWindow.h"

#include "PluginInstrument.h"

#include <utility>

namespace ihub::plugins
{
namespace
{
/** Background used behind an editor that does not fill the whole window. */
const juce::Colour pluginWindowBackground { 0xff1e1e24 };

std::atomic<int> livePluginWindows { 0 };
} // namespace

//==============================================================================
/** Message-thread lifetime token shared with the callbacks that may outlive
    this controller (the instrument's before-unload hook and the window's
    asynchronous close request). When the controller dies, `alive` is cleared
    before anything else, so those callbacks become no-ops. */
struct PluginWindow::Lifetime
{
    PluginWindow* owner = nullptr;
    bool alive = false;
};

//==============================================================================
/** The actual desktop window. It owns the editor as its content component, so
    both are destroyed by one `delete` from `PluginWindow::close()`. */
class PluginWindow::Window final : public juce::DocumentWindow
{
public:
    Window (const juce::String& windowTitle, std::function<void()> userClosed)
        : juce::DocumentWindow (windowTitle,
                                pluginWindowBackground,
                                juce::DocumentWindow::allButtons),
          onUserClosed (std::move (userClosed))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);

        liveWindowCount.fetch_add (1, std::memory_order_relaxed);
    }

    ~Window() override
    {
        liveWindowCount.fetch_sub (1, std::memory_order_relaxed);
    }

    void closeButtonPressed() override
    {
        if (closeAlreadyRequested)
            return;

        closeAlreadyRequested = true;

        // The handler destroys this window, which must not happen while its
        // own close-button callback is still on the stack.
        if (onUserClosed)
        {
            auto callback = onUserClosed;
            juce::MessageManager::callAsync ([callback] { callback(); });
        }
    }

    void userTriedToCloseWindow() override
    {
        closeButtonPressed();
    }

    static int getLiveWindowCount() noexcept
    {
        return liveWindowCount.load (std::memory_order_relaxed);
    }

private:
    std::function<void()> onUserClosed;
    bool closeAlreadyRequested = false;

    inline static std::atomic<int> liveWindowCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Window)
};

//==============================================================================
PluginWindow::PluginWindow()
    : lifetime (std::make_shared<Lifetime>())
{
    lifetime->owner = this;
    lifetime->alive = true;

    livePluginWindows.fetch_add (1, std::memory_order_relaxed);
}

PluginWindow::~PluginWindow()
{
    // From here on, callbacks still held by the instrument or the message
    // queue must not reach this object.
    lifetime->alive = false;

    close();

    livePluginWindows.fetch_sub (1, std::memory_order_relaxed);
}

//==============================================================================
bool PluginWindow::open (juce::AudioProcessor& processorToShow,
                         const juce::String& title,
                         juce::Component* owner,
                         juce::String* error)
{
    // Showing a processor directly cannot rely on an instrument's unload hook.
    if (instrument != nullptr)
        clearInstrumentCallback();

    const bool opened = showEditor (processorToShow, title, owner);

    if (error != nullptr)
        *error = opened ? juce::String() : lastError;

    return opened;
}

bool PluginWindow::open (PluginInstrument& instrumentToShow,
                         const juce::String& title,
                         juce::Component* owner,
                         juce::String* error)
{
    auto* processorToShow = instrumentToShow.getPlugin();

    if (processorToShow == nullptr)
    {
        close();
        lastError = "the plugin instance is not loaded";
        if (error != nullptr)
            *error = lastError;
        return false;
    }

    // A different instance is being shown: drop the old unload hook first, so
    // the retired instrument cannot close the new window later.
    if (instrument != nullptr && instrument != &instrumentToShow)
        clearInstrumentCallback();

    // Close the editor while the processor is still alive. unload() moves the
    // callback out before invoking it, so clearing it from close() is safe.
    std::weak_ptr<Lifetime> weakLifetime = lifetime;

    instrumentToShow.setBeforeUnloadCallback ([weakLifetime]
    {
        if (auto locked = weakLifetime.lock();
            locked != nullptr && locked->alive && locked->owner != nullptr)
        {
            locked->owner->close();
        }
    });

    instrument = &instrumentToShow;

    if (! showEditor (*processorToShow, title, owner))
    {
        clearInstrumentCallback();
        if (error != nullptr)
            *error = lastError;
        return false;
    }

    if (error != nullptr)
        error->clear();

    return true;
}

bool PluginWindow::showEditor (juce::AudioProcessor& processorToShow,
                               const juce::String& title,
                               juce::Component* owner)
{
    lastError.clear();

    // Reusing the window keeps a repeat click from flickering the editor.
    if (window != nullptr && processor == &processorToShow)
    {
        window->toFront (true);
        return true;
    }

    // Replacing the editor must not invoke onClose(): the caller may be inside
    // a member function of this object.
    destroyWindow();

    const auto pluginName = processorToShow.getName();

    if (! processorToShow.hasEditor())
    {
        lastError = pluginName.isNotEmpty()
                        ? "\"" + pluginName + "\" does not provide an editor"
                        : "the plugin does not provide an editor";
        return false;
    }

    // Registers the editor as the processor's active editor and asserts if one
    // is already open. showEditor() closed ours; a leak here is the caller's.
    auto* editor = processorToShow.createEditorAndMakeActive();

    if (editor == nullptr)
    {
        lastError = pluginName.isNotEmpty()
                        ? "\"" + pluginName + "\" failed to create its editor window"
                        : "the plugin failed to create its editor window";
        return false;
    }

    std::weak_ptr<Lifetime> weakLifetime = lifetime;

    auto newWindow = std::make_unique<Window> (
        title.isNotEmpty() ? title : pluginName,
        [weakLifetime]
        {
            if (auto locked = weakLifetime.lock();
                locked != nullptr && locked->alive && locked->owner != nullptr)
            {
                locked->owner->close();
            }
        });

    const int editorWidth = juce::jmax (1, editor->getWidth());
    const int editorHeight = juce::jmax (1, editor->getHeight());

    // The window owns the editor from this point on.
    newWindow->setContentOwned (editor, false);
    newWindow->setResizable (editor->isResizable(), false);

    if (owner != nullptr && owner->isShowing())
    {
        const auto ownerBounds = owner->getScreenBounds();

        newWindow->setTopLeftPosition (ownerBounds.getCentreX() - editorWidth / 2,
                                       ownerBounds.getCentreY() - editorHeight / 2);
        newWindow->setSize (editorWidth, editorHeight);
    }
    else
    {
        newWindow->centreWithSize (editorWidth, editorHeight);
    }

    window = std::move (newWindow);
    processor = &processorToShow;

    window->setVisible (true);
    window->toFront (true);

    return true;
}

void PluginWindow::clearInstrumentCallback() noexcept
{
    if (instrument == nullptr)
        return;

    auto* previous = instrument;
    instrument = nullptr;

    // A destroyed wrapper is not reachable here: this is the before-unload
    // path, and the instrument owned by the session outlives the window.
    previous->setBeforeUnloadCallback (nullptr);
}

void PluginWindow::destroyWindow()
{
    if (window == nullptr)
    {
        processor = nullptr;
        return;
    }

    window->setVisible (false);
    window.reset();
    processor = nullptr;
}

void PluginWindow::close()
{
    const bool wasOpen = window != nullptr;

    // Catch a final edit while the processor is still alive and the editor
    // still exists. The callback must not destroy this object (the session
    // only captures a state blob), but keep the same guarded copy as onClose.
    if (wasOpen && onBeforeClose && lifetime != nullptr && lifetime->alive)
    {
        auto callback = onBeforeClose;
        callback();
    }

    destroyWindow();

    // Detach from the instrument on every close. Re-opening installs a fresh
    // hook; leaving a stale one would let an old unload close a new window.
    clearInstrumentCallback();

    // The callback may destroy this object. Nothing is touched afterwards, and
    // it is deliberately not called from the destructor (Lifetime is cleared).
    if (wasOpen && onClose && lifetime != nullptr && lifetime->alive)
    {
        auto callback = onClose;
        callback();
    }
}

//==============================================================================
juce::AudioProcessorEditor* PluginWindow::getEditor() const noexcept
{
    if (window == nullptr)
        return nullptr;

    return dynamic_cast<juce::AudioProcessorEditor*> (window->getContentComponent());
}

juce::Component* PluginWindow::getWindowComponent() const noexcept
{
    return window.get();
}

int PluginWindow::getOpenWindowCount() noexcept
{
    return Window::getLiveWindowCount();
}

int PluginWindow::getLiveInstanceCount() noexcept
{
    return livePluginWindows.load (std::memory_order_relaxed);
}

} // namespace ihub::plugins
