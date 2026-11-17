#pragma once

#include "Transport.h"

#include <atomic>
#include <cstdint>

namespace AudioEngine {

class ClockIn final {
 public:
  static constexpr int pulsesPerQuarterNote = 24;

  static constexpr int queueCapacity = 256;

  static constexpr int tempoHistorySize = 32;

  static constexpr int minTempoIntervals = 8;

  static constexpr int lockTicks = 24;

  static constexpr double tempoSmoothing = 0.05;

  enum class Type : std::uint8_t {
    clock = 0xF8,
    start = 0xFA,
    continue_ = 0xFB,
    stop = 0xFC
  };

  struct Snapshot {
    bool locked = false;
    bool playing = false;
    bool clockPresent = false;

    double bpm = 0.0;

    double ppq = 0.0;

    std::int64_t tickCount = 0;

    std::int64_t clockCount = 0;

    double secondsSinceClock = 0.0;
  };

  ClockIn() = default;
  ~ClockIn() = default;

  ClockIn(const ClockIn&) = delete;
  ClockIn& operator=(const ClockIn&) = delete;

  void reset() noexcept;

  int clearQueue() noexcept;

  bool pushRealtime(std::uint8_t statusByte, double timestampSeconds) noexcept;

  void process(Transport& transport, int numSamples) noexcept;

  bool isLocked() const noexcept;

  bool isPlaying() const noexcept;

  bool isClockPresent() const noexcept;

  double getBpm() const noexcept;

  double getPpq() const noexcept;

  std::int64_t getTickCount() const noexcept;

  std::int64_t getClockCount() const noexcept;

  int getPendingEventCount() const noexcept;

  std::int64_t getDroppedEventCount() const noexcept;

  Snapshot snapshot() const noexcept;

 private:
  struct Event {
    Type type = Type::clock;
    double timestampSeconds = 0.0;
  };

  bool push(const Event& event) noexcept;
  int drain(Event* destination, int capacity) noexcept;

  void handleEvent(const Event& event) noexcept;
  void resetTempoRun() noexcept;
  void addInterval(double seconds) noexcept;
  double medianInterval() const noexcept;

  void loseClock() noexcept;

  static double clampBpm(double bpm) noexcept;
  static double tickPeriodSeconds(double bpm) noexcept;

  Event queue[queueCapacity]{};
  std::atomic<int> queueReadIndex{0};
  std::atomic<int> queueWriteIndex{0};
  std::atomic<std::int64_t> droppedEvents{0};

  std::atomic<bool> locked{false};
  std::atomic<bool> playing{false};
  std::atomic<bool> clockPresent{false};
  std::atomic<double> bpm{0.0};
  std::atomic<double> ppq{0.0};
  std::atomic<std::int64_t> tickCount{0};
  std::atomic<std::int64_t> clockCount{0};
  std::atomic<double> secondsSinceClock{0.0};

  double estimateBpm = 0.0;
  double intervalHistory[tempoHistorySize]{};
  int intervalCount = 0;
  int intervalHead = 0;

  bool haveTimestamp = false;
  double lastTimestamp = 0.0;

  bool seenClock = false;
  bool playingInternal = false;
  bool lockedInternal = false;

  std::int64_t positionTicks = 0;
  std::int64_t clockCountInternal = 0;
  double samplesSinceTick = 0.0;
  double sinceClockSeconds = 0.0;
};

}  // namespace AudioEngine
