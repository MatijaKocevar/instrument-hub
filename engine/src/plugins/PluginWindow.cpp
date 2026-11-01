#include "PluginWindow.h"

#include "PluginInstrument.h"
#include "window/PluginEditorWindow.h"
#include "window/PluginWindowLifetime.h"

#include <utility>

namespace ihub::plugins
{
namespace
{
std::atomic<int> livePluginWindows { 0 };
} // namespace

//==============================================================================
PluginWindow::PluginWindow()
    : lifetime (std::make_shared<WindowCallbackToken>())
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
    std::weak_ptr<WindowCallbackToken> weakLifetime = lifetime;

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
    // it is deliberately not called from the destructor (`alive` is cleared).
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
    return PluginEditorWindow::getLiveWindowCount();
}

int PluginWindow::getLiveInstanceCount() noexcept
{
    return livePluginWindows.load (std::memory_order_relaxed);
}

} // namespace ihub::plugins
