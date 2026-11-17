#include "Session.h"

#include <algorithm>
#include <cmath>

namespace ihub {

Api::AudioMeasurementState Session::measureLatency() {
  Api::AudioMeasurementState result;

  if (!prepared || engine == nullptr || !devices.isOpen()) {
    result.error = "the audio device is not open";
    return result;
  }

  if (devices.getManager().getActiveInputChannels().isZero()) {
    result.error =
        "the selected device has no active input channels; "
        "connect an output to an input";
    return result;
  }

  if (latencyProbeInput.size() <
      static_cast<std::size_t>(latencyProbeCaptureSamples)) {
    result.error = "the latency probe buffer is not ready";
    return result;
  }

  bool expected = false;

  if (!latencyProbeBusy.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel)) {
    result.error = "a latency measurement is already running";
    return result;
  }

  struct BusyGuard {
    std::atomic<bool>& busy;

    ~BusyGuard() { busy.store(false, std::memory_order_release); }
  } guard{latencyProbeBusy};

  std::fill(latencyProbeInput.begin(), latencyProbeInput.end(), 0.0f);

  latencyProbeInputChannels.store(0, std::memory_order_relaxed);
  latencyProbePosition.store(0, std::memory_order_relaxed);
  latencyProbeEmit.store(false, std::memory_order_relaxed);
  latencyProbeComplete.store(false, std::memory_order_release);
  latencyProbeRequested.store(true, std::memory_order_release);

  result.sampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
  result.bufferSize = devices.getCurrentBufferSize() > 0
                          ? devices.getCurrentBufferSize()
                          : maximumBlockSize;
  result.expectedMs = result.bufferSize * 2000.0 / result.sampleRate;

  const auto deadline = juce::Time::getMillisecondCounterHiRes() + 2000.0;

  while (!latencyProbeComplete.load(std::memory_order_acquire) &&
         juce::Time::getMillisecondCounterHiRes() < deadline)
    juce::Thread::sleep(2);

  latencyProbeRequested.store(false, std::memory_order_release);

  if (!latencyProbeComplete.load(std::memory_order_acquire)) {
    latencyProbeActive.store(false, std::memory_order_release);
    result.error = "the audio device did not deliver a capture block";
    return result;
  }

  if (latencyProbeInputChannels.load(std::memory_order_relaxed) <= 0) {
    result.error =
        "the selected device has no active input channels; "
        "connect an output to an input";
    return result;
  }

  int peakIndex = -1;
  float peak = 0.0f;

  for (int index = 0; index < latencyProbeCaptureSamples; ++index) {
    const auto value =
        std::abs(latencyProbeInput[static_cast<std::size_t>(index)]);

    if (value > peak) {
      peak = value;
      peakIndex = index;
    }
  }

  if (peakIndex < 0 || peak < 0.001f) {
    result.error =
        "no impulse found; connect an output to an input on the selected "
        "device";
    return result;
  }

  result.valid = true;
  result.roundTripSamples = peakIndex;
  result.roundTripMs =
      static_cast<double>(peakIndex) * 1000.0 / result.sampleRate;
  result.confidence = juce::jlimit(0.0f, 1.0f, peak / 0.2f);
  return result;
}

}  // namespace ihub
