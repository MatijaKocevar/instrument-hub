#include "Editor.h"
#include "Fixtures.h"

#include "instruments/PluginFx.h"

#include <iostream>
#include <string>

namespace PluginFxTest
{

void runEditor (juce::AudioPluginFormatManager& manager,
                const juce::PluginDescription& description)
{
    std::cout << "\n-- the native editor opens and closes with the wrapper --" << std::endl;

    ihub::plugins::PluginFx fx;
    juce::String error;

    check (fx.load (manager, description, kSampleRate, kBlockSize, error),
           "the editor host loads TestPlugin");

    if (! fx.isLoaded())
        return;

    check (fx.openEditor ("TestPlugin FX"), "openEditor creates the native editor");
    check (fx.isEditorOpen(), "the editor reports open");
    check (fx.getEditor() != nullptr, "the editor component exists");
    check (fx.getEditorWindow() != nullptr, "the editor has an app-owned window");

    pumpMessages (150);

    check (fx.isEditorOpen(), "the editor is still open after message-loop time");

    fx.closeEditor();
    check (! fx.isEditorOpen(), "closeEditor destroys the editor window");
    check (fx.getEditor() == nullptr, "no editor remains after close");

    check (fx.openEditor ("TestPlugin FX"), "the editor can be reopened");

    fx.unload();

    check (! fx.isEditorOpen(), "unload() closes the editor before destroying the plugin");
    check (! fx.isLoaded(), "unload() leaves the wrapper empty");
    check (fx.getProcessor() == nullptr, "no processor remains after unload");
}

} // namespace PluginFxTest
