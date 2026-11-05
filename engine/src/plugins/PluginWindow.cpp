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
}

PluginWindow::PluginWindow()
    : lifetime (std::make_shared<WindowCallbackToken>())
{
    lifetime->owner = this;
    lifetime->alive = true;

    livePluginWindows.fetch_add (1, std::memory_order_relaxed);
}

PluginWindow::~PluginWindow()
{
    lifetime->alive = false;

    destroyWindow();
    clearInstrumentCallback();

    livePluginWindows.fetch_sub (1, std::memory_order_relaxed);
}

bool PluginWindow::open (juce::AudioProcessor& processorToShow,
                         const juce::String& title,
                         juce::Component* owner,
                         juce::String* error)
{
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

    if (instrument != nullptr && instrument != &instrumentToShow)
        clearInstrumentCallback();

    std::weak_ptr<WindowCallbackToken> weakLifetime = lifetime;

    instrumentToShow.setBeforeUnloadCallback ([weakLifetime]
    {
        if (auto locked = weakLifetime.lock();
            locked != nullptr && locked->alive && locked->owner != nullptr)
        {
            locked->owner->closeForUnload();
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

    previous->setBeforeUnloadCallback (nullptr);
}

void PluginWindow::close()
{
    const bool wasOpen = openState;

    if (! wasOpen)
        return;

    openState = false;
    openWindowCount.fetch_sub (1, std::memory_order_relaxed);

    if (onBeforeClose && lifetime != nullptr && lifetime->alive)
    {
        auto callback = onBeforeClose;
        callback();
    }

    destroyWindow();
    clearInstrumentCallback();

    if (onClose && lifetime != nullptr && lifetime->alive)
    {
        auto callback = onClose;
        callback();
    }
}

void PluginWindow::closeForUnload()
{
    if (openState)
    {
        openState = false;
        openWindowCount.fetch_sub (1, std::memory_order_relaxed);
    }

    destroyWindow();
    clearInstrumentCallback();
}

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
    return openWindowCount.load (std::memory_order_relaxed);
}

int PluginWindow::getLiveInstanceCount() noexcept
{
    return livePluginWindows.load (std::memory_order_relaxed);
}

}
