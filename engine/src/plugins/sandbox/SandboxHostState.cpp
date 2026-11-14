#include "../SandboxHost.h"

#include <cstdio>

namespace ihub::plugins
{
    namespace
    {
        constexpr int defaultControlTimeoutMs = 3000;

        juce::File makeStateTransferFile ()
        {
            return juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("ihub-sandbox-state-" + juce::Uuid ().toString () + ".bin");
        }
    }

    bool SandboxHost::setStateInformation (const void* data, int numBytes)
    {
        if (data == nullptr || numBytes <= 0)
            return false;

        const juce::MemoryBlock block (data, static_cast<std::size_t> (numBytes));
        return setStateInformation (block);
    }

    bool SandboxHost::setStateInformation (const juce::MemoryBlock& block)
    {
        if (block.getSize () == 0)
            return false;

        juce::MemoryBlock ignored;

        if (!runControlCommand (
                sandbox::ControlCommand::applyState, block, ignored, defaultControlTimeoutMs))
            return false;

        commitState (block, false);
        return true;
    }

    juce::MemoryBlock SandboxHost::getStateInformation ()
    {
        juce::MemoryBlock state;
        juce::MemoryBlock ignored;

        if (runControlCommand (
                sandbox::ControlCommand::captureState, ignored, state, defaultControlTimeoutMs) &&
            state.getSize () > 0)
        {
            commitState (state, false);
            return state;
        }

        return {};
    }

    juce::String SandboxHost::readControlError () const
    {
        if (shared == nullptr)
            return {};

        return juce::String::fromUTF8 (shared->control.error).trim ();
    }

    int SandboxHost::getChildProcessId () const noexcept
    {
        if (shared == nullptr)
            return 0;

        return sandbox::loadRelaxed (shared->childProcessId);
    }

    bool SandboxHost::runControlCommand (sandbox::ControlCommand command,
                                         const juce::MemoryBlock& input,
                                         juce::MemoryBlock& output,
                                         int timeoutMs)
    {
        output.reset ();

        const juce::ScopedLock lock (controlLock);

        if (shared == nullptr || childProcess == nullptr || !isRunning () ||
            !childProcess->isRunning ())
            return false;

        stateTransferFile = makeStateTransferFile ();
        stateTransferFile.deleteFile ();

        if (command == sandbox::ControlCommand::applyState ||
            command == sandbox::ControlCommand::applyParameters)
        {
            if (!stateTransferFile.replaceWithData (input.getData (), input.getSize ()))
                return false;
        }

        std::snprintf (shared->control.statePath,
                       sizeof (shared->control.statePath),
                       "%s",
                       stateTransferFile.getFullPathName ().toRawUTF8 ());

        std::snprintf (shared->control.error, sizeof (shared->control.error), "%s", "");

        shared->control.command = static_cast<std::int32_t> (command);
        shared->control.stateBytes = static_cast<std::int32_t> (input.getSize ());
        shared->control.status = static_cast<std::int32_t> (sandbox::ControlStatus::pending);

        const auto sequence = sandbox::loadRelaxed (shared->control.sequence) + 1;
        sandbox::storeRelease (shared->control.sequence, sequence);

        const auto deadline = juce::Time::getMillisecondCounterHiRes () +
                              static_cast<double> (juce::jmax (1, timeoutMs));

        bool completed = false;

        for (;;)
        {
            const auto controlStatus =
                static_cast<sandbox::ControlStatus> (sandbox::loadAcquire (shared->control.status));

            if (controlStatus == sandbox::ControlStatus::done)
            {
                completed = true;
                break;
            }

            if (controlStatus == sandbox::ControlStatus::failed)
                break;

            if (childProcess == nullptr || !childProcess->isRunning ())
                break;

            if (juce::Time::getMillisecondCounterHiRes () > deadline)
                break;

            juce::Thread::sleep (1);
        }

        if (completed && command == sandbox::ControlCommand::captureState &&
            stateTransferFile.existsAsFile ())
            completed = stateTransferFile.loadFileAsData (output);

        stateTransferFile.deleteFile ();
        stateTransferFile = juce::File ();
        shared->control.command = static_cast<std::int32_t> (sandbox::ControlCommand::none);

        return completed &&
               (command == sandbox::ControlCommand::applyState ||
                command == sandbox::ControlCommand::applyParameters ||
                command == sandbox::ControlCommand::openEditor ||
                command == sandbox::ControlCommand::closeEditor || output.getSize () > 0);
    }

}
