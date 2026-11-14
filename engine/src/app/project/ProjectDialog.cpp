#include "ProjectDialog.h"

#include "ProjectPaths.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace ihub::app
{

    namespace
    {

        juce::File startingFileFor (const Api::ProjectDialogRequest& request)
        {
            auto folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                              .getChildFile ("Music")
                              .getChildFile ("Instrument Hub");

            if (!folder.isDirectory ())
                folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

            if (!request.currentPath.empty ())
            {
                const auto current = expandProjectPath (juce::String (request.currentPath));

                if (current.getParentDirectory ().isDirectory ())
                    return current;
            }

            if (request.mode != "save")
                return folder;

            juce::String name = request.suggestedName.empty ()
                                    ? juce::String ("Untitled")
                                    : juce::String (request.suggestedName);

            if (!name.endsWithIgnoreCase (".ihub"))
                name += ".ihub";

            return folder.getChildFile (name);
        }

    }

    struct ProjectDialog::Shared
    {
        std::uint64_t id = 0;
        std::string state = "pending";
        std::string path;

        mutable std::mutex mutex;
    };

    ProjectDialog::~ProjectDialog () = default;

    bool ProjectDialog::begin (const Api::ProjectDialogRequest& request,
                               std::uint64_t& id,
                               std::string& error)
    {
        if (juce::MessageManager::getInstanceWithoutCreating () == nullptr)
        {
            error = "the user interface is not running";
            return false;
        }

        std::shared_ptr<Shared> dialog;

        {
            const std::lock_guard<std::mutex> lock (mutex);

            if (current != nullptr)
            {
                const std::lock_guard<std::mutex> dialogLock (current->mutex);

                if (current->state == "pending")
                {
                    error = "a file dialog is already open";
                    return false;
                }
            }

            dialog = std::make_shared<Shared> ();
            dialog->id = nextId++;
            current = dialog;
            id = dialog->id;
        }

        const bool save = request.mode == "save";

        const int flags = save ? juce::FileBrowserComponent::saveMode |
                                     juce::FileBrowserComponent::canSelectFiles |
                                     juce::FileBrowserComponent::warnAboutOverwriting
                               : juce::FileBrowserComponent::openMode |
                                     juce::FileBrowserComponent::canSelectFiles;

        const juce::String title =
            save ? "Save Instrument Hub Project" : "Open Instrument Hub Project";

        const auto startingFile = startingFileFor (request);

        const bool posted = juce::MessageManager::callAsync (
            [dialog, startingFile, title, flags, save] () mutable
            {
                auto chooser =
                    std::make_shared<juce::FileChooser> (title, startingFile, "*.ihub", true);

                auto keepAlive = std::make_shared<std::shared_ptr<juce::FileChooser>> (chooser);

                chooser->launchAsync (
                    flags,
                    [dialog, save, keepAlive] (const juce::FileChooser& result)
                    {
                        auto file = result.getResult ();

                        if (save && file != juce::File () && !file.hasFileExtension ("ihub"))
                        {
                            file = file.withFileExtension (".ihub");
                        }

                        {
                            const std::lock_guard<std::mutex> lock (dialog->mutex);
                            dialog->state = file == juce::File () ? "cancelled" : "done";
                            dialog->path = file == juce::File ()
                                               ? std::string ()
                                               : file.getFullPathName ().toStdString ();
                        }

                        juce::MessageManager::callAsync ([keepAlive] () mutable
                                                         { keepAlive->reset (); });
                    });
            });

        if (!posted)
        {
            const std::lock_guard<std::mutex> lock (dialog->mutex);
            dialog->state = "cancelled";
            error = "could not open the file dialog";
            return false;
        }

        return true;
    }

    bool ProjectDialog::poll (std::uint64_t id,
                              Api::ProjectDialogStatus& status,
                              std::string& error) const
    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (current == nullptr || current->id != id)
        {
            error = "unknown file dialog";
            return false;
        }

        const std::lock_guard<std::mutex> dialogLock (current->mutex);

        status.id = current->id;
        status.state = current->state;
        status.path = current->path;
        return true;
    }

}
