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
                    std::uint32_t& lastSequence)
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

        case sandbox::ControlCommand::none:
            break;
    }

    setControlStatus (shared, sandbox::ControlStatus::failed);
}

}
