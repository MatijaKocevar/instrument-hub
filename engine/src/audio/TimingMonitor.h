#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

namespace AudioEngine {

struct TimingSnapshot {
  std::uint64_t callbacks = 0;
  std::uint64_t overruns = 0;
  std::uint64_t droppedBlocks = 0;
  std::uint64_t samplesProcessed = 0;

  double sampleRate = 0.0;
  int blockSize = 0;

  double deadlineMs = 0.0;

  double lastCallbackMs = 0.0;
  double rollingMaxCallbackMs = 0.0;
  double worstCallbackMs = 0.0;
  double averageCallbackMs = 0.0;
  double lastLoadPercent = 0.0;

  int windowSize = 0;
};

class TimingMonitor final {
 public:
  static constexpr int defaultWindowSize = 512;

  struct CallbackToken {
    std::chrono::steady_clock::time_point started;
  };

  explicit TimingMonitor(int windowSize = defaultWindowSize);
  ~TimingMonitor() = default;

  TimingMonitor(const TimingMonitor&) = delete;
  TimingMonitor& operator=(const TimingMonitor&) = delete;

  void prepare(double sampleRate, int blockSize) noexcept;

  void setDeadlineMs(double newDeadlineMs) noexcept;

  void reset() noexcept;

  CallbackToken beginCallback() const noexcept;

  void endCallback(CallbackToken token, int numSamplesProcessed = -1) noexcept;

  void recordCallback(double elapsedMs, int numSamplesProcessed = -1) noexcept;

  double getDeadlineMs() const noexcept;

  std::uint64_t getCallbackCount() const noexcept;
  std::uint64_t getOverrunCount() const noexcept;

  std::uint64_t getXrunCount() const noexcept { return getOverrunCount(); }

  std::uint64_t getDroppedBlockCount() const noexcept;
  std::uint64_t getSamplesProcessed() const noexcept;

  double getLastCallbackMs() const noexcept;
  double getRollingMaxCallbackMs() const noexcept;
  double getWorstCallbackMs() const noexcept;
  double getAverageCallbackMs() const noexcept;

  double getLastLoadPercent() const noexcept;

  int getWindowSize() const noexcept { return windowSize; }

  TimingSnapshot snapshot() const noexcept;

 private:
  struct WindowEntry {
    std::uint64_t sequence = 0;
    double milliseconds = 0.0;
  };

  void pushWindow(double milliseconds) noexcept;

  const int windowSize;
  std::vector<WindowEntry> window;
  int windowHead = 0;
  int windowTail = 0;
  int windowCount = 0;
  std::uint64_t sequence = 0;

  std::atomic<double> sampleRate{0.0};
  std::atomic<int> blockSize{0};
  std::atomic<double> deadlineMs{0.0};

  std::atomic<std::uint64_t> callbacks{0};
  std::atomic<std::uint64_t> overruns{0};
  std::atomic<std::uint64_t> droppedBlocks{0};
  std::atomic<std::uint64_t> samplesProcessed{0};

  std::atomic<double> lastCallbackMs{0.0};
  std::atomic<double> rollingMaxCallbackMs{0.0};
  std::atomic<double> worstCallbackMs{0.0};
  std::atomic<double> totalCallbackMs{0.0};
};

}  // namespace AudioEngine
