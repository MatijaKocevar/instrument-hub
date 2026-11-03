#include "../SandboxHost.h"

#include <juce_core/juce_core.h>

namespace ihub::plugins
{
namespace
{
constexpr int parameterControlTimeoutMs = 1000;
}

bool SandboxHost::setParameters (const std::vector<ParameterEdit>& edits)
{
    if (edits.empty())
        return true;

    if (edits.size() > static_cast<std::size_t> (sandbox::maximumParameterBatch))
        return false;

    juce::MemoryOutputStream stream;
    stream.writeInt (static_cast<int> (edits.size()));

    for (const auto& edit : edits)
    {
        stream.writeString (edit.id);
        stream.writeFloat (edit.normalisedValue);
    }

    const juce::MemoryBlock block (stream.getData(), stream.getDataSize());
    juce::MemoryBlock ignored;

    return runControlCommand (sandbox::ControlCommand::applyParameters,
                              block,
                              ignored,
                              parameterControlTimeoutMs);
}

}
