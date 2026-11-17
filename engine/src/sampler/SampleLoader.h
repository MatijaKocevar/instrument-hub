#pragma once

#include "SampleData.h"
#include "SampleHandoff.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

namespace ihub::sampler {

class SampleLoader : private juce::Thread {
 public:
  using Ptr = std::shared_ptr<const SampleData>;

  struct Event {
    int requestId = 0;
    bool success = false;
    juce::String path;
    juce::String error;
  };

  SampleLoader();
  ~SampleLoader() override;

  bool prepare(double engineSampleRate) noexcept;
  double getEngineSampleRate() const noexcept;

  static Ptr loadBlocking(const juce::File& file, double engineSampleRate,
                          juce::String& error);

  int loadAsync(const juce::File& file, SampleHandoff& destination,
                juce::String& error);

  int loadAsync(const juce::File& file, juce::String& error);

  SampleHandoff& getHandoff() noexcept { return handoff; }
  const SampleHandoff& getHandoff() const noexcept { return handoff; }

  const SampleData* poll() noexcept { return handoff.poll(); }

  const SampleData* current() const noexcept { return handoff.current(); }

  void collectGarbage() noexcept { handoff.collectGarbage(); }

  bool isBusy() const noexcept {
    return pending.load(std::memory_order_acquire) > 0;
  }
  bool isIdle() const noexcept { return !isBusy(); }

  bool takeEvent(Event& event);

  bool waitForIdle(int timeoutMs = 10000);

  int getNumPending() const noexcept {
    return pending.load(std::memory_order_acquire);
  }

  int getNumDroppedEvents() const noexcept {
    return droppedEvents.load(std::memory_order_relaxed);
  }

  static constexpr double maximumDurationSeconds = 900.0;

  static constexpr juce::int64 maximumSourceFrames = 200'000'000;

 private:
  struct Job {
    int id = 0;
    juce::File file;
    double engineSampleRate = 48000.0;
    SampleHandoff* destination = nullptr;
  };

  static constexpr int maximumQueuedJobs = 64;
  static constexpr int eventCapacity = 32;

  void run() override;

  void pushEvent(Event event);
  void finishJob(int id, const juce::File& file,
                 std::unique_ptr<SampleData> sample, juce::String error,
                 SampleHandoff& destination);

  std::atomic<double> engineSampleRate{48000.0};
  std::atomic<int> pending{0};
  std::atomic<int> nextRequestId{1};
  std::atomic<int> droppedEvents{0};

  std::mutex queueMutex;
  std::deque<Job> jobs;
  juce::WaitableEvent wakeUp{false};

  SampleHandoff handoff;

  juce::AbstractFifo eventFifo{eventCapacity};
  Event events[eventCapacity];

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleLoader)
};

}  // namespace ihub::sampler
