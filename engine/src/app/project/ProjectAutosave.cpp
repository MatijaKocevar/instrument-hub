#include "ProjectAutosave.h"

#include "project/Serializer.h"

#include <utility>

namespace ihub::app
{

ProjectAutosave::ProjectAutosave (juce::File projectFile, int debounceMilliseconds)
    : file (std::move (projectFile)),
      debounceMs (juce::jmax (1, debounceMilliseconds)),
      saved (file.existsAsFile() && file.hasWriteAccess())
{
}

ProjectAutosave::~ProjectAutosave()
{
    shutdown();
}

void ProjectAutosave::suspend()
{
    stopTimer();

    const std::lock_guard<std::mutex> operation (operationMutex);
    const std::lock_guard<std::mutex> lock (mutex);
    suspended = true;
}

void ProjectAutosave::resume()
{
    const std::lock_guard<std::mutex> operation (operationMutex);
    const std::lock_guard<std::mutex> lock (mutex);
    suspended = false;
}

void ProjectAutosave::shutdown()
{
    stopTimer();

    const std::lock_guard<std::mutex> operation (operationMutex);
    const std::lock_guard<std::mutex> lock (mutex);
    active = false;
    suspended = true;
}

void ProjectAutosave::setCaptureHandler (CaptureHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    capture = std::move (handler);
}

void ProjectAutosave::setStateChangedHandler (StateChangedHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    stateChanged = std::move (handler);
}

void ProjectAutosave::noteEdit (const char* kind, int slot)
{
    juce::ignoreUnused (kind);

    if (file == juce::File())
        return;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! active || suspended)
            return;

        dirty = true;
        saved = false;
        ++generation;

        if (slot >= 0)
            touchedSlots.insert (slot);
    }

    startTimer (debounceMs);
}

bool ProjectAutosave::saveNow (juce::String* errorOut, bool force)
{
    stopTimer();

    const std::lock_guard<std::mutex> operation (operationMutex);

    CaptureHandler handler;
    juce::File target;
    std::vector<int> slots;
    std::uint64_t saveGeneration = 0;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! active)
        {
            if (errorOut != nullptr)
                *errorOut = "the project is closed";

            return false;
        }

        if (! force && ! dirty)
        {
            if (errorOut != nullptr)
                errorOut->clear();

            return true;
        }

        if (suspended)
        {
            if (errorOut != nullptr)
                *errorOut = "the project is suspended";

            return false;
        }

        handler = capture;
        target = file;
        slots.assign (touchedSlots.begin(), touchedSlots.end());
        saveGeneration = generation;
    }

    if (target == juce::File())
    {
        const juce::String message ("no project file is open");

        {
            const std::lock_guard<std::mutex> lock (mutex);
            error = message.toStdString();
            saved = false;
        }

        if (errorOut != nullptr)
            *errorOut = message;

        notifyStateChanged();
        return false;
    }

    if (target.existsAsFile() && ! target.hasWriteAccess())
    {
        const juce::String message ("the project is read-only");

        {
            const std::lock_guard<std::mutex> lock (mutex);
            error = message.toStdString();
            saved = false;
        }

        if (errorOut != nullptr)
            *errorOut = message;

        notifyStateChanged();
        return false;
    }

    if (handler == nullptr)
    {
        const juce::String message ("the project cannot be captured");

        {
            const std::lock_guard<std::mutex> lock (mutex);
            error = message.toStdString();
            saved = false;
        }

        if (errorOut != nullptr)
            *errorOut = message;

        notifyStateChanged();
        return false;
    }

    project::SetFile set;
    juce::String captureError;

    if (! handler (set, slots, captureError))
    {
        {
            const std::lock_guard<std::mutex> lock (mutex);
            error = captureError.isNotEmpty()
                        ? captureError.toStdString()
                        : std::string ("could not capture the project");
            saved = false;
        }

        if (errorOut != nullptr)
            *errorOut = captureError.isNotEmpty() ? captureError
                                                  : juce::String ("could not capture the project");

        notifyStateChanged();
        return false;
    }

    juce::String saveError;

    if (! project::Serializer::save (set, target, saveError))
    {
        {
            const std::lock_guard<std::mutex> lock (mutex);
            error = saveError.toStdString();
            saved = false;
        }

        if (errorOut != nullptr)
            *errorOut = saveError;

        notifyStateChanged();
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (mutex);
        ++saveCount;
        saved = true;
        error.clear();

        if (generation == saveGeneration)
        {
            dirty = false;
            touchedSlots.clear();
        }
        else
        {
            dirty = true;
        }
    }

    if (errorOut != nullptr)
        errorOut->clear();

    notifyStateChanged();
    return true;
}

Api::ProjectSaveState ProjectAutosave::getState() const
{
    Api::ProjectSaveState state;
    const std::lock_guard<std::mutex> lock (mutex);

    state.path = file.getFullPathName().toStdString();
    state.name = file.getFileNameWithoutExtension().toStdString();
    state.dirty = dirty;
    state.saved = saved;
    state.error = error;
    return state;
}

int ProjectAutosave::getSaveCount() const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);
    return saveCount;
}

bool ProjectAutosave::hasPendingEdits() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return dirty;
}

void ProjectAutosave::timerCallback()
{
    stopTimer();

    juce::String saveError;
    const bool savedNow = saveNow (&saveError);

    if (! savedNow && saveError.isNotEmpty())
        juce::Logger::writeToLog ("project: autosave failed: " + saveError);
}

void ProjectAutosave::notifyStateChanged()
{
    StateChangedHandler handler;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        handler = stateChanged;
    }

    if (handler != nullptr)
        handler();
}

}
