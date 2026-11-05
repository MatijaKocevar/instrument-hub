#include "plugins/PluginWindow.h"
#include "plugins/PluginInstrument.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

void pumpMessages (int milliseconds)
{
    auto* messageManager = juce::MessageManager::getInstance();
    const auto deadline = juce::Time::getMillisecondCounter()
                          + static_cast<juce::uint32> (juce::jmax (0, milliseconds));

    while (juce::Time::getMillisecondCounter() < deadline)
        messageManager->runDispatchLoopUntil (10);
}

int countDistinctColours (const juce::Image& image)
{
    std::set<juce::uint32> colours;

    const int stepX = juce::jmax (1, image.getWidth() / 64);
    const int stepY = juce::jmax (1, image.getHeight() / 64);

    for (int y = 0; y < image.getHeight(); y += stepY)
        for (int x = 0; x < image.getWidth(); x += stepX)
            colours.insert (image.getPixelAt (x, y).getARGB());

    return static_cast<int> (colours.size());
}

juce::XEmbedComponent* findEmbeddedEditorHost (juce::Component& component)
{
    if (auto* embedded = dynamic_cast<juce::XEmbedComponent*> (&component))
        return embedded;

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (auto* found = findEmbeddedEditorHost (*component.getChildComponent (i)))
            return found;

    return nullptr;
}

juce::PluginDescription* findTestPlugin (juce::AudioPluginFormatManager& manager,
                                         const juce::File& bundle,
                                         juce::OwnedArray<juce::PluginDescription>& types)
{
    const auto path = bundle.getFullPathName();

    for (auto* format : manager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    for (auto* type : types)
        if (type->name.contains ("TestPlugin"))
            return type;

    return nullptr;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();

    check (display != nullptr, "a display is available (DISPLAY=:0)");

    if (display == nullptr)
        return 1;

    std::cout << "display: " << display->userBounds.toString() << std::endl;

    const juce::File bundle = argc > 1
        ? juce::File (juce::File (argv[1]).getFullPathName())
        : juce::File (IHUB_TESTPLUGIN_VST3);

    std::cout << "bundle: " << bundle.getFullPathName() << std::endl;

    check (bundle.isDirectory(), "TestPlugin.vst3 bundle exists");

    if (! bundle.isDirectory())
        return 1;

    juce::AudioPluginFormatManager formatManager;
    juce::addDefaultFormatsToManager (formatManager);

    juce::OwnedArray<juce::PluginDescription> types;
    auto* type = findTestPlugin (formatManager, bundle, types);

    check (type != nullptr, "TestPlugin type is enumerated by the format manager");

    if (type == nullptr)
        return 1;

    const juce::PluginDescription description = *type;
    juce::String error;

    ihub::plugins::PluginInstrument instrument;

    check (instrument.load (formatManager, description, 48000.0, 512, error),
           "PluginInstrument::load succeeded"
               + (error.isEmpty() ? std::string() : " (" + error.toStdString() + ")"));

    if (! instrument.isLoaded())
        return 1;

    check (instrument.getPlugin()->hasEditor(), "TestPlugin reports a native editor");

    {
        ihub::plugins::PluginWindow window;

        check (window.open (instrument, "TestPlugin Editor"),
               "PluginWindow::open() created the editor window");

        auto* editor = window.getEditor();

        check (window.isOpen(), "the window reports itself open");
        check (editor != nullptr, "the window exposes the AudioProcessorEditor");
        check (editor != nullptr && editor->isShowing(), "the editor is showing");
        check (window.getWindowComponent() != nullptr
                   && window.getWindowComponent()->isOnDesktop(),
               "the window is on the desktop");
        check (window.getWindowComponent() != nullptr
                   && window.getWindowComponent()->isVisible(),
               "the window is visible");
        check (window.getProcessor() == instrument.getPlugin(),
               "the window shows the loaded processor");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
               "exactly one plugin window is open");

        if (editor != nullptr)
        {
            std::cout << "editor: " << editor->getWidth() << "x" << editor->getHeight()
                      << " \"" << editor->getName() << "\"" << std::endl;
        }

        pumpMessages (2000);

        check (window.isOpen(), "the editor is still open after 2 s");
        check (editor != nullptr && editor->isShowing(),
               "the editor is still showing after 2 s");

        if (editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0)
        {
            const auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());

            const auto colours = countDistinctColours (snapshot);

            std::cout << "snapshot: " << snapshot.getWidth() << "x"
                      << snapshot.getHeight() << ", " << colours
                      << " distinct colours (native X11 editor content is not in"
                         " the JUCE component tree)" << std::endl;

            check (! snapshot.isNull(), "createComponentSnapshot returned an image");

            const auto snapshotFile = juce::File::getCurrentWorkingDirectory()
                                          .getChildFile ("plugin-window-editor.png");

            if (auto stream = snapshotFile.createOutputStream())
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (snapshot, *stream);
                std::cout << "snapshot: wrote " << snapshotFile.getFullPathName() << std::endl;
            }

            auto* embeddedHost = findEmbeddedEditorHost (*editor);

            check (embeddedHost != nullptr,
                   "the VST3 editor uses a native X11 embedding component");
            check (embeddedHost != nullptr && embeddedHost->getHostWindowID() != 0,
                   "the native editor host window exists");
            check (embeddedHost != nullptr && embeddedHost->isShowing(),
                   "the native editor host is showing");
        }

        window.setHideOnClose (true);
        window.close();

        check (! window.isOpen(), "close() closed the window");
        check (window.getEditor() == nullptr, "close() destroyed the editor");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
               "no plugin window is left open");
        check (instrument.getPlugin()->getActiveEditor() == nullptr,
               "the processor no longer has an active editor");

        check (window.open (instrument, "TestPlugin Editor"),
               "the window can be reopened after close()");

        auto* reopenedEditor = window.getEditor();
        check (reopenedEditor != nullptr && reopenedEditor->isShowing(),
               "the reopened editor is showing");

        instrument.unload();

        check (! instrument.isLoaded(), "unload() released the plugin instance");
        check (! window.isOpen(), "unload() closed the editor window before the processor died");
        check (window.getEditor() == nullptr, "unload() destroyed the editor");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
               "no plugin window survives unload()");
    }

    check (ihub::plugins::PluginWindow::getLiveInstanceCount() == 0,
           "the PluginWindow controller was destroyed");

    juce::String reloadError;
    check (instrument.load (formatManager, description, 48000.0, 512, reloadError),
           "the instrument loads again after unload()");

    {
        ihub::plugins::PluginWindow window;

        check (window.open (instrument, "TestPlugin Editor"),
               "the editor opens on the second instance");
        check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
               "the reopened window is on screen");
    }

    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
           "destroying the controller closed its window");
    check (instrument.getPlugin()->getActiveEditor() == nullptr,
           "the processor has no active editor after the controller died");

    instrument.unload();

    check (ihub::plugins::PluginWindow::getLiveInstanceCount() == 0,
           "no PluginWindow leaked");

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
