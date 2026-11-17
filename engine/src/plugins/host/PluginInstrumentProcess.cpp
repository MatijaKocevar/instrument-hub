#include "../PluginInstrument.h"

namespace ihub::plugins {
void PluginInstrument::process(juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer& midi) {
  if (plugin == nullptr) {
    buffer.clear();
    clearPendingMidi();
    return;
  }

  const auto numSamples = buffer.getNumSamples();

  if (numSamples <= 0) return;

  const auto bufferChannels = buffer.getNumChannels();
  const auto pluginChannels = juce::jmax(plugin->getTotalNumInputChannels(),
                                         plugin->getTotalNumOutputChannels());

  if (pluginChannels <= 0) {
    buffer.clear();
    clearPendingMidi();
    return;
  }

  juce::MidiBuffer* midiToUse = &midi;

  const auto read = pendingReadIndex.load(std::memory_order_relaxed);
  const auto write = pendingWriteIndex.load(std::memory_order_acquire);

  if (read != write) {
    midiScratch.clear();
    midiScratch.addEvents(midi, 0, numSamples, 0);
    drainPendingMidi(midiScratch);
    midiToUse = &midiScratch;
  }

  if (pluginChannels == bufferChannels) {
    plugin->processBlock(buffer, *midiToUse);
    return;
  }

  if (pluginChannels < bufferChannels) {
    juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(),
                                  pluginChannels, numSamples);
    plugin->processBlock(view, *midiToUse);

    for (int channel = pluginChannels; channel < bufferChannels; ++channel)
      buffer.clear(channel, 0, numSamples);

    return;
  }

  if (wideChannels < pluginChannels ||
      wideScratch.getNumSamples() < numSamples) {
    buffer.clear();
    return;
  }

  juce::AudioBuffer<float> wide(wideScratch.getArrayOfWritePointers(),
                                pluginChannels, numSamples);
  wide.clear();
  plugin->processBlock(wide, *midiToUse);

  for (int channel = 0; channel < bufferChannels; ++channel)
    buffer.copyFrom(channel, 0, wide, channel, 0, numSamples);
}

}  // namespace ihub::plugins
