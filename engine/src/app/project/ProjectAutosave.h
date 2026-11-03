#pragma once

#include "api/state/ProjectState.h"

#include <juce_events/juce_events.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace ihub::project
{
struct SetFile;
}

namespace ihub::app
{

class ProjectAutosave final : private juce::Timer
{
public:
    using CaptureHandler =
        std::function<bool (project::SetFile&, const std::vector<int>& touchedSlots,
                            juce::String& error)>;

    explicit ProjectAutosave (juce::File projectFile, int debounceMs = 500);
    ~ProjectAutosave() override;

    ProjectAutosave (const ProjectAutosave&) = delete;
    ProjectAutosave& operator= (const ProjectAutosave&) = delete;

    void setCaptureHandler (CaptureHandler handler);

    using StateChangedHandler = std::function<void()>;
    void setStateChangedHandler (StateChangedHandler handler);

    void noteEdit (const char* kind, int slot);

    bool saveNow (juce::String* error = nullptr, bool force = false);

    void suspend();
    void resume();
    void shutdown();

    Api::ProjectSaveState getState() const;

    int getSaveCount() const noexcept;

    bool hasPendingEdits() const;

private:
    void timerCallback() override;
    void notifyStateChanged();

    mutable std::mutex mutex;
    mutable std::mutex operationMutex;
    const juce::File file;
    const int debounceMs;

    CaptureHandler capture;
    StateChangedHandler stateChanged;

    bool active = true;
    bool suspended = false;
    bool dirty = false;
    bool saved = false;
    std::string error;
    std::set<int> touchedSlots;
    std::uint64_t generation = 0;
    int saveCount = 0;
};

}
