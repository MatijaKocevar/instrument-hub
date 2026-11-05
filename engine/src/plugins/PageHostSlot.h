#pragma once

#include "PageHost.h"
#include "SandboxSlotControl.h"

#include <memory>

namespace ihub::plugins
{

class PageHostSlot final : public Instrument,
                           public AudioEngine::SlotSource,
                           public SandboxSlotControl
{
public:
    PageHostSlot (std::shared_ptr<PageHost> pageHost, int logicalSlot);

    int getSlotIndex() const noexcept { return slot; }
    int getInternalIndex() const noexcept { return internalIndex; }

    bool isRunning() const noexcept override;
    bool isFailed() const noexcept override;
    juce::String getFailureReason() const override;

    bool setStateInformation (const void* data, int numBytes) override;
    bool setStateInformation (const juce::MemoryBlock& block) override;
    juce::MemoryBlock getStateInformation() override;

    juce::MemoryBlock getLastCommittedState() const override;
    std::uint64_t getLastCommittedStateRevision() const noexcept override;
    bool flushPublishedState() override;

    bool setParameters (const std::vector<ParameterEdit>& edits) override;

    bool openEditor (juce::String& error) override;
    bool closeEditor (juce::String& error) override;
    bool isEditorOpen() const noexcept override;

    int getChildProcessId() const noexcept override;
    int getPluginLatencySamples() const noexcept override;
    int getUnderrunCount() const noexcept override;
    int getDroppedRequestCount() const noexcept override;
    int getDroppedMidiEventCount() const noexcept override;
    std::int64_t getPlayedBlockCount() const noexcept override;

    void prepare (double sampleRate, int maximumBlockSize) override;

    void noteOn (int ch, int note, float vel) override;
    void noteOff (int ch, int note) override;

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    int latencySamples() const noexcept override;

    Instrument::Kind kind() const noexcept override { return Instrument::Kind::Plugin; }

    const std::shared_ptr<PageHost>& getPageHost() const noexcept { return host; }

private:
    std::shared_ptr<PageHost> host;
    const int slot;
    const int internalIndex;
};

} // namespace ihub::plugins
