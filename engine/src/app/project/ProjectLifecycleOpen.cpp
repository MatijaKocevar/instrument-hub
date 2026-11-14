#include "ProjectLifecycle.h"

#include "ProjectAutosave.h"
#include "ProjectImport.h"
#include "ProjectPaths.h"
#include "ProjectSession.h"

#include "Session.h"
#include "api/SessionBridge.h"
#include "project/Serializer.h"

#include <utility>

namespace ihub::app
{

    bool ProjectLifecycle::openProject (const juce::File& file, bool remember, std::string& error)
    {
        if (!file.existsAsFile ())
        {
            error = "project does not exist: " + file.getFullPathName ().toStdString ();
            return false;
        }

        auto set = std::make_shared<project::SetFile> ();
        juce::String loadError;

        if (!project::Serializer::load (file, *set, loadError))
        {
            error = loadError.toStdString ();
            return false;
        }

        ihub::SessionConfig config;
        juce::String configError;

        if (!loadProjectSession (file, config, configError))
        {
            error = configError.toStdString ();
            return false;
        }

        const auto previousAutosave = currentAutosave ();

        if (previousAutosave != nullptr)
        {
            juce::String saveError;

            if (previousAutosave->hasPendingEdits () && !previousAutosave->saveNow (&saveError))
            {
                error = saveError.isNotEmpty ()
                            ? saveError.toStdString ()
                            : std::string ("could not save the current project");
                return false;
            }

            previousAutosave->suspend ();
        }

        const auto previousConfig = session.getConfig ();
        juce::String rebuildError;

        if (!rebuildSession (config, rebuildError))
        {
            juce::String restoreError;
            (void)rebuildSession (previousConfig, restoreError);

            if (previousAutosave != nullptr)
                previousAutosave->resume ();

            error = rebuildError.toStdString ();
            return false;
        }

        installAutosave (file, std::move (set));
        recordProject (file, remember);
        notifyChanged ();

        OpenedHandler handler;

        {
            handler = openedHandler;
        }

        if (handler != nullptr)
            handler (file);

        error.clear ();
        return true;
    }

    bool ProjectLifecycle::newProject (const juce::String& path,
                                       const juce::String& name,
                                       std::string& error)
    {
        auto file = path.isEmpty () ? uniqueUntitledFile () : expandProjectPath (path);

        if (file == juce::File ())
        {
            error = "could not choose a project path";
            return false;
        }

        if (file.existsAsFile ())
        {
            error = "project already exists: " + file.getFullPathName ().toStdString ();
            return false;
        }

        juce::String createError;

        if (!writeBundledProject (file, name, createError))
        {
            error = createError.toStdString ();
            return false;
        }

        return openProject (file, true, error);
    }

    bool ProjectLifecycle::rebuildSession (const ihub::SessionConfig& config, juce::String& error)
    {
        const bool wasOpen = session.isDeviceOpen ();
        const auto setup = session.getDevices ().getCurrentSetup ();

        ihub::SessionConfig next = config;

        if (wasOpen)
        {
            next.device.deviceTypeName = setup.deviceTypeName;
            next.device.outputDeviceName = setup.outputDeviceName;
            next.device.inputDeviceName = setup.inputDeviceName;
            next.device.sampleRate =
                setup.sampleRate > 0.0 ? setup.sampleRate : next.device.sampleRate;
            next.device.bufferSize =
                setup.bufferSize > 0 ? setup.bufferSize : next.device.bufferSize;
            next.device.numOutputChannels = setup.numOutputChannels > 0
                                                ? setup.numOutputChannels
                                                : next.device.numOutputChannels;
            next.device.numInputChannels = setup.numInputChannels;
        }

        if (!session.build (next, error, wasOpen))
            return false;

        if (session.getNumSlots () > 0)
            session.armSlot (0);

        if (wasOpen && !session.isDeviceOpen ())
        {
            juce::String openError;

            if (!session.openDevice (&openError))
                juce::Logger::writeToLog ("project: could not reopen the audio device: " +
                                          openError);
        }

        Api::attachSession (state, server, session);
        return true;
    }

    juce::File ProjectLifecycle::uniqueUntitledFile () const
    {
        const auto defaults = defaultProjectFile ();
        const auto directory = defaults.getParentDirectory ();

        if (!directory.exists () && !directory.createDirectory ())
            return {};

        if (!defaults.existsAsFile ())
            return defaults;

        for (int index = 2; index < 1000; ++index)
        {
            const auto candidate = directory.getChildFile (defaults.getFileNameWithoutExtension () +
                                                           " " + juce::String (index) + ".ihub");

            if (!candidate.existsAsFile ())
                return candidate;
        }

        return {};
    }

}
