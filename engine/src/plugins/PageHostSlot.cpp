#include "PageHostSlot.h"

#include <utility>

namespace ihub::plugins {

PageHostSlot::PageHostSlot(std::shared_ptr<PageHost> pageHost, int logicalSlot)
    : host(std::move(pageHost)),
      slot(logicalSlot),
      internalIndex(host != nullptr ? host->findInternalIndex(logicalSlot)
                                    : -1) {}

bool PageHostSlot::isRunning() const noexcept {
  return host != nullptr && host->isReady() && internalIndex >= 0;
}

bool PageHostSlot::isFailed() const noexcept {
  return host == nullptr || host->isFailed() ||
         (internalIndex >= 0 && host->getSlotStatus(slot).failed);
}

juce::String PageHostSlot::getFailureReason() const {
  if (host == nullptr) return "the shared plugin page is not available";

  const auto status = host->getSlotStatus(slot);

  if (status.error.isNotEmpty()) return status.error;

  return host->getFailureReason();
}

bool PageHostSlot::setStateInformation(const void* data, int numBytes) {
  return host != nullptr && host->setStateInformation(slot, data, numBytes);
}

bool PageHostSlot::setStateInformation(const juce::MemoryBlock& block) {
  return host != nullptr && host->setStateInformation(slot, block);
}

juce::MemoryBlock PageHostSlot::getStateInformation() {
  return host != nullptr ? host->getStateInformation(slot)
                         : juce::MemoryBlock();
}

juce::MemoryBlock PageHostSlot::getLastCommittedState() const {
  return host != nullptr ? host->getLastCommittedState(slot)
                         : juce::MemoryBlock();
}

std::uint64_t PageHostSlot::getLastCommittedStateRevision() const noexcept {
  return host != nullptr ? host->getLastCommittedStateRevision(slot) : 0;
}

bool PageHostSlot::flushPublishedState() {
  return host != nullptr && host->flushPublishedState(slot);
}

bool PageHostSlot::setParameters(const std::vector<ParameterEdit>& edits) {
  return host != nullptr && host->setParameters(slot, edits);
}

bool PageHostSlot::openEditor(juce::String& error) {
  if (host == nullptr) {
    error = "the shared plugin page is not available";
    return false;
  }

  return host->openEditor(slot, error);
}

bool PageHostSlot::closeEditor(juce::String& error) {
  if (host == nullptr) {
    error = "the shared plugin page is not available";
    return false;
  }

  return host->closeEditor(slot, error);
}

bool PageHostSlot::isEditorOpen() const noexcept {
  return host != nullptr && host->isEditorOpen(slot);
}

int PageHostSlot::getChildProcessId() const noexcept {
  return host != nullptr ? host->getChildProcessId() : 0;
}

int PageHostSlot::getPluginLatencySamples() const noexcept {
  return host != nullptr ? host->getSlotPluginLatencySamples(slot) : 0;
}

int PageHostSlot::getUnderrunCount() const noexcept {
  return host != nullptr ? host->getSlotUnderrunCount(slot) : 0;
}

int PageHostSlot::getDroppedRequestCount() const noexcept {
  return host != nullptr ? host->getSlotDroppedRequestCount(slot) : 0;
}

int PageHostSlot::getDroppedMidiEventCount() const noexcept {
  return host != nullptr ? host->getSlotDroppedMidiEventCount(slot) : 0;
}

std::int64_t PageHostSlot::getPlayedBlockCount() const noexcept {
  return host != nullptr ? host->getSlotPlayedBlockCount(slot) : 0;
}

void PageHostSlot::prepare(double sampleRate, int maximumBlockSize) {
  if (host != nullptr) host->prepareSlot(slot, sampleRate, maximumBlockSize);
}

void PageHostSlot::noteOn(int ch, int note, float vel) {
  if (host != nullptr) host->noteOn(slot, ch, note, vel);
}

void PageHostSlot::noteOff(int ch, int note) {
  if (host != nullptr) host->noteOff(slot, ch, note);
}

void PageHostSlot::process(juce::AudioBuffer<float>& buffer,
                           juce::MidiBuffer& midi) {
  if (host != nullptr)
    host->processSlot(slot, internalIndex, buffer, midi);
  else
    buffer.clear();
}

int PageHostSlot::latencySamples() const noexcept {
  return host != nullptr ? host->getSlotLatencySamples(slot) : 0;
}

}  // namespace ihub::plugins
