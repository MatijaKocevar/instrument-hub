#include "../SandboxHost.h"

namespace ihub::plugins
{

namespace
{
constexpr int openEditorTimeoutMs = 30000;
constexpr int closeEditorTimeoutMs = 5000;
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
        error = "the plugin host could not open the editor";
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
        error = "the plugin host could not close the editor";
        return false;
    }

    return true;
}

bool SandboxHost::isEditorOpen() const noexcept
{
    if (shared == nullptr || ! isRunning())
        return false;

    return sandbox::loadAcquire (shared->editorOpen) != 0;
}

}
