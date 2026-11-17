#include "ProjectLifecycle.h"

#include "ProjectAutosave.h"
#include "ProjectCapture.h"

#include "project/Serializer.h"

#include <utility>

namespace ihub::app {

bool ProjectLifecycle::saveAs(const juce::File& file, std::string& error) {
    const auto current = currentAutosave();

    if (current == nullptr) {
        error = "no project is open";
        return false;
    }

    auto replacement = std::make_shared<ProjectAutosave>(file);
    auto captureBase = base;

    replacement->setCaptureHandler([this, captureBase](project::SetFile& set,
                                                       const std::vector<int>& touchedSlots,
                                                       juce::String& captureError) {
        return captureProject(session, state, *captureBase, touchedSlots, set, captureError);
    });

    replacement->setStateChangedHandler([this] { notifyChanged(); });

    juce::String saveError;

    if (!replacement->saveNow(&saveError, true)) {
        error = saveError.isNotEmpty() ? saveError.toStdString()
                                       : std::string("could not save the project");
        return false;
    }

    {
        const std::lock_guard<std::mutex> stateLock(stateMutex);
        autosave = replacement;
        closed = false;
        errorState = {};
    }

    current->shutdown();

    if (auto loaded = std::make_shared<project::SetFile>();
        project::Serializer::load(file, *loaded, saveError))
        base = std::move(loaded);

    recordProject(file, true);
    notifyChanged();
    error.clear();
    return true;
}

bool ProjectLifecycle::saveCurrent(std::string& error) {
    const auto current = currentAutosave();

    if (current == nullptr) {
        error = "no project is open";
        return false;
    }

    juce::String saveError;

    if (!current->saveNow(&saveError, true)) {
        error = saveError.isNotEmpty() ? saveError.toStdString()
                                       : std::string("could not save the project");
        return false;
    }

    error.clear();
    return true;
}

bool ProjectLifecycle::closeProject(std::string& error) {
    const auto current = currentAutosave();

    if (current == nullptr) {
        error = "no project is open";
        return false;
    }

    juce::String saveError;

    if (current->hasPendingEdits() && !current->saveNow(&saveError)) {
        error = saveError.isNotEmpty() ? saveError.toStdString()
                                       : std::string("could not save the project");
        return false;
    }

    {
        const std::lock_guard<std::mutex> stateLock(stateMutex);
        autosave.reset();
        base.reset();
        closed = true;
        errorState = {};
    }

    current->shutdown();
    notifyChanged();
    error.clear();
    return true;
}

}  // namespace ihub::app
