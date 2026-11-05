#include "../SandboxHost.h"

namespace ihub::plugins
{

namespace
{
constexpr int openEditorTimeoutMs = 10000;
constexpr int closeEditorTimeoutMs = 5000;

juce::String editorFailureReason (const sandbox::SharedMemory* shared,
                                  const juce::String& fallback)
{
    if (shared == nullptr)
        return fallback;

    const auto detail = juce::String::fromUTF8 (shared->control.error).trim();
    return detail.isNotEmpty() ? detail : fallback;
}
}

bool SandboxHost::openEditor (juce::String& error)
{
    error.clear();

    if (! isRunning())
    {
        error = "the plugin host is not running";
        return false;
    }

    juce::MemoryBlock ignored;

    if (! runControlCommand (sandbox::ControlCommand::openEditor,
                             ignored,
                             ignored,
                             openEditorTimeoutMs))
    {
        const auto controlStatus =
            shared != nullptr
                ? static_cast<sandbox::ControlStatus> (
                      sandbox::loadAcquire (shared->control.status))
                : sandbox::ControlStatus::failed;

        if (controlStatus == sandbox::ControlStatus::pending)
        {
            const juce::String reason {
                "the plugin host did not answer the editor request"
            };

            markFailed (reason);
            error = reason;
            return false;
        }

        error = editorFailureReason (shared,
                                     "the plugin host could not open the editor");
        return false;
    }

    return true;
}

bool SandboxHost::closeEditor (juce::String& error)
{
    error.clear();

    if (! isRunning())
    {
        error = "the plugin host is not running";
        return false;
    }

    juce::MemoryBlock ignored;

    if (! runControlCommand (sandbox::ControlCommand::closeEditor,
                             ignored,
                             ignored,
                             closeEditorTimeoutMs))
    {
        const auto controlStatus =
            shared != nullptr
                ? static_cast<sandbox::ControlStatus> (
                      sandbox::loadAcquire (shared->control.status))
                : sandbox::ControlStatus::failed;

        if (controlStatus == sandbox::ControlStatus::pending)
        {
            const juce::String reason {
                "the plugin host did not answer the editor close request"
            };

            markFailed (reason);
            error = reason;
            return false;
        }

        error = editorFailureReason (shared,
                                     "the plugin host could not close the editor");
        return false;
    }

    (void) flushPublishedState();
    return true;
}

bool SandboxHost::isEditorOpen() const noexcept
{
    if (shared == nullptr || ! isRunning())
        return false;

    return sandbox::loadAcquire (shared->editorOpen) != 0;
}

}
