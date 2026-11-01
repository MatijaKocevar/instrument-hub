#include "../PluginWindow.h"

#include "PluginEditorWindow.h"
#include "PluginWindowLifetime.h"

#include <utility>

namespace ihub::plugins
{
bool PluginWindow::showEditor (juce::AudioProcessor& processorToShow,
                               const juce::String& title,
                               juce::Component* owner)
{
    lastError.clear();

    if (window != nullptr && processor == &processorToShow)
    {
        window->toFront (true);
        return true;
    }

    destroyWindow();

    const auto pluginName = processorToShow.getName();

    if (! processorToShow.hasEditor())
    {
        lastError = pluginName.isNotEmpty()
                        ? "\"" + pluginName + "\" does not provide an editor"
                        : "the plugin does not provide an editor";
        return false;
    }

    auto* editor = processorToShow.createEditorAndMakeActive();

    if (editor == nullptr)
    {
        lastError = pluginName.isNotEmpty()
                        ? "\"" + pluginName + "\" failed to create its editor window"
                        : "the plugin failed to create its editor window";
        return false;
    }

    std::weak_ptr<WindowCallbackToken> weakLifetime = lifetime;

    auto newWindow = std::make_unique<PluginEditorWindow> (
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

}
