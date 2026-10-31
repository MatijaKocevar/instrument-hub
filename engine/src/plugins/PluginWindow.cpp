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
                         juce::Component* owner)
{
    return showEditor (processorToShow, title, owner);
}

bool PluginWindow::open (PluginInstrument& instrument,
                         const juce::String& title,
                         juce::Component* owner)
{
    auto* processorToShow = instrument.getPlugin();

    if (processorToShow == nullptr)
    {
        close();
        return false;
    }

    // Close the editor while the processor is still alive. unload() moves the
    // callback out before invoking it, so clearing it from close() is safe.
    std::weak_ptr<Lifetime> weakLifetime = lifetime;

    instrument.setBeforeUnloadCallback ([weakLifetime]
    {
        if (auto locked = weakLifetime.lock();
            locked != nullptr && locked->alive && locked->owner != nullptr)
        {
            locked->owner->close();
        }
    });

    if (! showEditor (*processorToShow, title, owner))
    {
        instrument.setBeforeUnloadCallback (nullptr);
        return false;
    }

    return true;
}

bool PluginWindow::showEditor (juce::AudioProcessor& processorToShow,
                               const juce::String& title,
                               juce::Component* owner)
{
    // Replacing the editor must not invoke onClose(): the caller may be inside
    // a member function of this object.
    destroyWindow();

    if (! processorToShow.hasEditor())
        return false;

    // Registers the editor as the processor's active editor and asserts if one
    // is already open. showEditor() closed ours; a leak here is the caller's.
    auto* editor = processorToShow.createEditorAndMakeActive();

    if (editor == nullptr)
        return false;

    std::weak_ptr<Lifetime> weakLifetime = lifetime;

    auto newWindow = std::make_unique<Window> (
        title.isNotEmpty() ? title : processorToShow.getName(),
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

    destroyWindow();

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
