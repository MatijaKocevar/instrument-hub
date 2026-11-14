#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

namespace ihub::plugins
{

    struct SandboxParameterEdit
    {
        juce::String id;
        float normalisedValue = 0.0f;
    };

    class SandboxSlotControl
    {
    public:
        using ParameterEdit = SandboxParameterEdit;

        virtual ~SandboxSlotControl () = default;

        virtual bool isRunning () const noexcept = 0;
        virtual bool isFailed () const noexcept = 0;
        virtual juce::String getFailureReason () const = 0;

        virtual bool setStateInformation (const void* data, int numBytes) = 0;
        virtual bool setStateInformation (const juce::MemoryBlock& block) = 0;
        virtual juce::MemoryBlock getStateInformation () = 0;

        virtual juce::MemoryBlock getLastCommittedState () const = 0;
        virtual std::uint64_t getLastCommittedStateRevision () const noexcept = 0;
        virtual bool flushPublishedState () = 0;

        virtual bool setParameters (const std::vector<ParameterEdit>& edits) = 0;

        virtual bool openEditor (juce::String& error) = 0;
        virtual bool closeEditor (juce::String& error) = 0;
        virtual bool isEditorOpen () const noexcept = 0;

        virtual int getChildProcessId () const noexcept = 0;
        virtual int getPluginLatencySamples () const noexcept = 0;
        virtual int getUnderrunCount () const noexcept = 0;
        virtual int getDroppedRequestCount () const noexcept = 0;
        virtual int getDroppedMidiEventCount () const noexcept = 0;
        virtual std::int64_t getPlayedBlockCount () const noexcept = 0;
    };

} // namespace ihub::plugins
