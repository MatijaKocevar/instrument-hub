#include "SandboxClientControl.h"

#include "../PluginInstrument.h"

#include <juce_core/juce_core.h>

namespace ihub::plugins::sandbox_client
{
namespace
{
juce::File stateFileFor (const sandbox::SharedMemory& shared)
{
    return juce::File (juce::String::fromUTF8 (shared.control.statePath));
}

void setControlStatus (sandbox::SharedMemory& shared,
                       sandbox::ControlStatus status) noexcept
{
    sandbox::storeRelease (shared.control.status,
                           static_cast<std::int32_t> (status));
}

void applyState (sandbox::SharedMemory& shared, PluginInstrument* plugin)
{
    const auto path = stateFileFor (shared);

    juce::MemoryBlock data;

    if (plugin == nullptr || ! path.existsAsFile() || ! path.loadFileAsData (data))
    {
        setControlStatus (shared, sandbox::ControlStatus::failed);
        return;
    }

    const auto expected = sandbox::loadRelaxed (shared.control.stateBytes);

    if (expected < 0 || static_cast<int> (data.getSize()) != expected)
    {
        setControlStatus (shared, sandbox::ControlStatus::failed);
        return;
    }

    setControlStatus (shared,
                      plugin->setStateInformation (data)
                          ? sandbox::ControlStatus::done
                          : sandbox::ControlStatus::failed);
}

void applyParameters (sandbox::SharedMemory& shared, PluginInstrument* plugin)
{
    const auto path = stateFileFor (shared);

    juce::MemoryBlock data;

    if (plugin == nullptr || ! path.existsAsFile() || ! path.loadFileAsData (data))
    {
        setControlStatus (shared, sandbox::ControlStatus::failed);
        return;
    }

    juce::MemoryInputStream stream (data, false);
    const auto count = stream.readInt();

    if (count <= 0 || count > sandbox::maximumParameterBatch)
    {
        setControlStatus (shared, sandbox::ControlStatus::failed);
        return;
    }

    for (int index = 0; index < count; ++index)
    {
        const auto id = stream.readString();

        if (id.isEmpty() || stream.getNumBytesRemaining() < static_cast<juce::int64> (sizeof (float)))
        {
            setControlStatus (shared, sandbox::ControlStatus::failed);
            return;
        }

        const auto value = stream.readFloat();

        if (! plugin->setParameterValue (id, value))
        {
            setControlStatus (shared, sandbox::ControlStatus::failed);
            return;
        }
    }

    setControlStatus (shared, sandbox::ControlStatus::done);
}

void captureState (sandbox::SharedMemory& shared, PluginInstrument* plugin)
{
    if (plugin == nullptr)
    {
        setControlStatus (shared, sandbox::ControlStatus::failed);
        return;
    }

    const auto data = plugin->getStateInformation();
    const auto path = stateFileFor (shared);

    if (data.getSize() == 0 || path == juce::File()
        || ! path.replaceWithData (data.getData(), data.getSize()))
    {
        setControlStatus (shared, sandbox::ControlStatus::failed);
        return;
    }

    sandbox::storeRelaxed (shared.control.stateBytes,
                           static_cast<std::int32_t> (data.getSize()));
    setControlStatus (shared, sandbox::ControlStatus::done);
}
}

void handleControl (sandbox::SharedMemory& shared,
                    PluginInstrument* plugin,
                    std::uint32_t& lastSequence,
                    EditorControl editorControl)
{
    const auto sequence = sandbox::loadAcquire (shared.control.sequence);

    if (sequence == lastSequence)
        return;

    lastSequence = sequence;
    setControlStatus (shared, sandbox::ControlStatus::pending);

    switch (static_cast<sandbox::ControlCommand> (
        sandbox::loadRelaxed (shared.control.command)))
    {
        case sandbox::ControlCommand::applyState:
            applyState (shared, plugin);
            return;

        case sandbox::ControlCommand::captureState:
            captureState (shared, plugin);
            return;

        case sandbox::ControlCommand::applyParameters:
            applyParameters (shared, plugin);
            return;

        case sandbox::ControlCommand::openEditor:
        case sandbox::ControlCommand::closeEditor:
        {
            if (! editorControl)
            {
                setControlStatus (shared, sandbox::ControlStatus::failed);
                return;
            }

            juce::String error;
            const bool open = sandbox::loadRelaxed (shared.control.command)
                              == static_cast<std::int32_t> (sandbox::ControlCommand::openEditor);

            setControlStatus (shared,
                              editorControl (open, error)
                                  ? sandbox::ControlStatus::done
                                  : sandbox::ControlStatus::failed);
            return;
        }

        case sandbox::ControlCommand::none:
            break;
    }

    setControlStatus (shared, sandbox::ControlStatus::failed);
}

}
