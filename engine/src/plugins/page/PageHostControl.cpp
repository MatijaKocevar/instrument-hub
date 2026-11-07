#include "../PageHost.h"

#include <cstdio>
#include <utility>

namespace ihub::plugins
{

juce::File PageHost::stateTransferFileFor (SlotState& slot) const
{
    auto& files = const_cast<PageHost*> (this)->stateTransferFiles;

    if (files.size() < slots.size())
        files.resize (slots.size());

    auto& file = files[static_cast<std::size_t> (slot.internalIndex)];

    if (file == juce::File())
        file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("ihub-page-state-" + juce::Uuid().toString() + ".bin");

    return file;
}

bool PageHost::runControlCommand (SlotState& slot,
                                  sandbox::ControlCommand command,
                                  const juce::MemoryBlock& input,
                                  juce::MemoryBlock& output,
                                  int timeoutMs)
{
    output.reset();

    const std::lock_guard<std::mutex> lock (lifecycleMutex);

    auto* shared = channelFor (slot);

    if (shared == nullptr || childProcess == nullptr || ! childProcess->isRunning())
        return false;

    auto file = stateTransferFileFor (slot);
    file.deleteFile();

    if (command == sandbox::ControlCommand::applyState
        || command == sandbox::ControlCommand::applyParameters)
    {
        if (! file.replaceWithData (input.getData(), input.getSize()))
            return false;
    }

    std::snprintf (shared->control.statePath,
                   sizeof (shared->control.statePath), "%s",
                   file.getFullPathName().toRawUTF8());
    std::snprintf (shared->control.error, sizeof (shared->control.error), "%s", "");

    slot.controlInFlight.fetch_add (1, std::memory_order_relaxed);

    shared->control.command = static_cast<std::int32_t> (command);
    shared->control.stateBytes = static_cast<std::int32_t> (input.getSize());
    shared->control.status = static_cast<std::int32_t> (sandbox::ControlStatus::pending);

    const auto sequence = sandbox::loadRelaxed (shared->control.sequence) + 1;
    sandbox::storeRelease (shared->control.sequence, sequence);

    const auto deadline = juce::Time::getMillisecondCounterHiRes()
                        + static_cast<double> (juce::jmax (1, timeoutMs));

    bool completed = false;

    for (;;)
    {
        const auto controlStatus = static_cast<sandbox::ControlStatus> (
            sandbox::loadAcquire (shared->control.status));

        if (controlStatus == sandbox::ControlStatus::done)
        {
            completed = true;
            break;
        }

        if (controlStatus == sandbox::ControlStatus::failed)
            break;

        if (childProcess == nullptr || ! childProcess->isRunning())
            break;

        if (abortRequested())
            break;

        if (juce::Time::getMillisecondCounterHiRes() > deadline)
            break;

        juce::Thread::sleep (1);
    }

    if (completed && command == sandbox::ControlCommand::captureState
        && file.existsAsFile())
    {
        completed = file.loadFileAsData (output);
    }

    file.deleteFile();
    shared->control.command = static_cast<std::int32_t> (sandbox::ControlCommand::none);

    const bool success = completed
                      && (command == sandbox::ControlCommand::applyState
                          || command == sandbox::ControlCommand::applyParameters
                          || command == sandbox::ControlCommand::openEditor
                          || command == sandbox::ControlCommand::closeEditor
                          || output.getSize() > 0);

    slot.controlInFlight.fetch_sub (1, std::memory_order_release);
    return success;
}

bool PageHost::setStateInformation (int slot, const juce::MemoryBlock& state)
{
    auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr || state.getSize() == 0)
        return false;

    juce::MemoryBlock ignored;

    if (! runControlCommand (*stateSlot, sandbox::ControlCommand::applyState,
                             state, ignored, options.controlTimeoutMs))
        return false;

    commitState (*stateSlot, state, false);
    return true;
}

bool PageHost::setStateInformation (int slot, const void* data, int numBytes)
{
    if (data == nullptr || numBytes <= 0)
        return false;

    return setStateInformation (slot,
                                juce::MemoryBlock (data, static_cast<std::size_t> (numBytes)));
}

juce::MemoryBlock PageHost::getStateInformation (int slot)
{
    auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr)
        return {};

    juce::MemoryBlock state;
    juce::MemoryBlock ignored;

    if (! runControlCommand (*stateSlot, sandbox::ControlCommand::captureState,
                             ignored, state, options.controlTimeoutMs)
        || state.getSize() == 0)
        return {};

    commitState (*stateSlot, state, false);
    return state;
}

juce::MemoryBlock PageHost::getLastCommittedState (int slot) const
{
    const auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr)
        return {};

    const std::lock_guard<std::mutex> lock (stateSlot->stateLock);
    return stateSlot->lastCommittedState;
}

std::uint64_t PageHost::getLastCommittedStateRevision (int slot) const noexcept
{
    if (const auto* stateSlot = findSlot (slot); stateSlot != nullptr)
    {
        const std::lock_guard<std::mutex> lock (stateSlot->stateLock);
        return stateSlot->lastCommittedRevision;
    }

    return 0;
}

bool PageHost::flushPublishedState (int slot)
{
    auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr)
        return false;

    auto* shared = channelFor (*stateSlot);

    if (shared == nullptr)
        return false;

    const auto published = sandbox::loadAcquire (shared->editorStateRevision);

    if (published == 0)
        return false;

    juce::File path;
    std::int32_t expectedBytes = 0;

    {
        const std::lock_guard<std::mutex> lock (stateSlot->stateLock);

        if (static_cast<std::uint32_t> (published) == stateSlot->seenEditorStateRevision)
            return false;

        stateSlot->seenEditorStateRevision = static_cast<std::uint32_t> (published);
        expectedBytes = sandbox::loadRelaxed (shared->editorStateBytes);
        path = juce::File (juce::String::fromUTF8 (shared->editorStatePath));
    }

    if (path == juce::File() || expectedBytes <= 0)
        return false;

    juce::MemoryBlock state;

    if (! path.existsAsFile() || ! path.loadFileAsData (state))
        return false;

    if (static_cast<std::int32_t> (state.getSize()) != expectedBytes)
        return false;

    commitState (*stateSlot, state, true);
    return true;
}

void PageHost::flushAllPublishedStates()
{
    for (auto& slot : slots)
        (void) flushPublishedState (slot->descriptor.slotIndex);
}

bool PageHost::setParameters (int slot, const std::vector<SandboxParameterEdit>& edits)
{
    auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr || edits.empty())
        return false;

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

    return runControlCommand (*stateSlot, sandbox::ControlCommand::applyParameters,
                              block, ignored, options.controlTimeoutMs);
}

bool PageHost::openEditor (int slot, juce::String& error)
{
    error.clear();

    auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr)
    {
        error = "the slot is not part of this plugin page";
        return false;
    }

    juce::MemoryBlock ignored;

    if (! runControlCommand (*stateSlot, sandbox::ControlCommand::openEditor,
                             ignored, ignored, 10000))
    {
        auto* shared = channelFor (*stateSlot);
        const auto controlStatus = shared != nullptr
                                       ? static_cast<sandbox::ControlStatus> (
                                             sandbox::loadAcquire (shared->control.status))
                                       : sandbox::ControlStatus::failed;

        if (controlStatus == sandbox::ControlStatus::pending)
        {
            error = "the plugin host did not answer the editor request";
            return false;
        }

        error = shared != nullptr ? juce::String::fromUTF8 (shared->control.error).trim()
                                  : juce::String();
        if (error.isEmpty())
            error = "the plugin host could not open the editor";

        return false;
    }

    return true;
}

bool PageHost::closeEditor (int slot, juce::String& error)
{
    error.clear();

    auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr)
    {
        error = "the slot is not part of this plugin page";
        return false;
    }

    juce::MemoryBlock ignored;

    if (! runControlCommand (*stateSlot, sandbox::ControlCommand::closeEditor,
                             ignored, ignored, 5000))
    {
        error = "the plugin host could not close the editor";
        return false;
    }

    (void) flushPublishedState (slot);
    return true;
}

bool PageHost::isEditorOpen (int slot) const noexcept
{
    const auto* stateSlot = findSlot (slot);

    if (stateSlot == nullptr)
        return false;

    auto* shared = getSlotChannel (stateSlot->internalIndex);
    return shared != nullptr && sandbox::loadAcquire (shared->editorOpen) != 0;
}

int PageHost::getOpenEditorCount() const noexcept
{
    int count = 0;

    for (const auto& slot : slots)
        if (isEditorOpen (slot->descriptor.slotIndex))
            ++count;

    return count;
}

void PageHost::commitState (SlotState& slot, const juce::MemoryBlock& state, bool notify)
{
    if (state.getSize() == 0)
        return;

    SlotStateCallback callback;
    std::uint64_t revision = 0;

    {
        const std::lock_guard<std::mutex> lock (slot.stateLock);
        slot.lastCommittedState = state;
        revision = ++slot.lastCommittedRevision;

        if (notify)
            callback = slot.stateCallback;
    }

    if (callback != nullptr)
    {
        try
        {
            callback (slot.descriptor.slotIndex, state, revision);
        }
        catch (...)
        {
        }
    }
}

} // namespace ihub::plugins
