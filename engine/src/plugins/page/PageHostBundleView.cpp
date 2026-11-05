#include "../PageHost.h"

#include <algorithm>

namespace ihub::plugins
{
namespace
{
bool cloneDirectoryTree (const juce::File& source, const juce::File& destination)
{
    if (! destination.createDirectory())
        return false;

    {
        juce::Array<juce::File> directories;
        source.findChildFiles (directories, juce::File::findDirectories, true, "*");

        for (const auto& directory : directories)
        {
            const auto relative = directory.getRelativePathFrom (source);

            if (relative.isEmpty() || relative.startsWith (".."))
                return false;

            if (! destination.getChildFile (relative).createDirectory())
                return false;
        }
    }

    {
        juce::Array<juce::File> files;
        source.findChildFiles (files, juce::File::findFiles, true, "*");

        for (const auto& file : files)
        {
            const auto relative = file.getRelativePathFrom (source);

            if (relative.isEmpty() || relative.startsWith (".."))
                return false;

            const auto link = destination.getChildFile (relative);

            if (! file.createSymbolicLink (link, true))
                return false;
        }
    }

    return true;
}

bool writeGroupConfig (const juce::File& bundle, const juce::String& group)
{
    juce::Array<juce::File> modules;
    bundle.findChildFiles (modules, juce::File::findFiles, true, "*.so");

    for (const auto& module : modules)
    {
        const auto config = module.getSiblingFile ("vstbridge.toml");
        const auto contents = "[\"*\"]\ngroup = \"" + group + "\"\n";

        if (! config.replaceWithText (contents))
            return false;
    }

    return ! modules.isEmpty();
}
}

bool PageHost::prepareBundleView (juce::String& error)
{
    bundleViewRoot = juce::File();
    bundleViews.clear();
    vstbridgeGroup.clear();

    const auto needsView = std::any_of (slots.begin(), slots.end(),
                                        [] (const std::unique_ptr<SlotState>& slot)
                                        {
                                            return slot->descriptor.pluginBundle != juce::File()
                                                && slot->descriptor.pluginBundle.getFullPathName()
                                                       .containsIgnoreCase ("vstbridge");
                                        });

    if (! needsView)
        return true;

    const auto parent = options.tempDirectory != juce::File()
                            ? options.tempDirectory
                            : juce::File::getSpecialLocation (juce::File::tempDirectory);

    vstbridgeGroup = "ihub-page-" + juce::Uuid().toString().removeCharacters ("-");
    bundleViewRoot = parent.getChildFile ("ihub-page-view-" + juce::Uuid().toString());

    if (! bundleViewRoot.createDirectory())
    {
        error = "could not create the plugin page view: "
              + bundleViewRoot.getFullPathName();
        bundleViewRoot = juce::File();
        return false;
    }

    for (const auto& slot : slots)
    {
        const auto source = slot->descriptor.pluginBundle;

        if (source == juce::File()
            || ! source.getFullPathName().containsIgnoreCase ("vstbridge"))
            continue;

        const auto key = source.getFullPathName();
        const auto existing = bundleViews.find (key);

        if (existing != bundleViews.end())
            continue;

        const auto view = bundleViewRoot
                              .getChildFile (juce::String (slot->descriptor.slotIndex))
                              .getChildFile (source.getFileName());

        if (! cloneDirectoryTree (source, view)
            || ! writeGroupConfig (view, vstbridgeGroup))
        {
            error = "could not prepare the grouped plugin view for "
                  + source.getFullPathName();
            removeBundleView();
            return false;
        }

        bundleViews[key] = view;
    }

    return true;
}

void PageHost::removeBundleView() noexcept
{
    bundleViews.clear();

    if (bundleViewRoot != juce::File())
    {
        bundleViewRoot.deleteRecursively();
        bundleViewRoot = juce::File();
    }

    vstbridgeGroup.clear();
}

} // namespace ihub::plugins
