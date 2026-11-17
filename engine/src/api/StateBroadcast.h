#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace httplib::ws {
class WebSocket;
}

namespace Api {

class StateStore;

enum class StateEventKind {
  initial = 0,
  pad,
  arm,
  settings,
  transport,
  slot,
  patch,
  warm,
  device,
  midi,
  pairing,
  project,
  track
};

struct PadMirror {
  bool pressed = false;
  double updatedMs = 0.0;
};

struct StateBroadcastOptions {
  int clientPollIntervalUs = 5000;

  static constexpr int maxPads = 64;
};

class StateBroadcast {
 public:
  explicit StateBroadcast(StateStore& state,
                          StateBroadcastOptions options = {});
  ~StateBroadcast();

  StateBroadcast(const StateBroadcast&) = delete;
  StateBroadcast& operator=(const StateBroadcast&) = delete;

  void start();
  void stop();

  bool isRunning() const noexcept;

  void publishPad(int pad, bool pressed);

  void publishStateChange(const char* kind, int slot = -1);

  void handleClient(httplib::ws::WebSocket& websocket);

  PadMirror getPad(int pad) const;

  std::uint64_t getEventCount() const noexcept;
  std::uint64_t getFrameCount() const noexcept;
  int getClientCount() const noexcept;
  std::uint64_t getLastSequence() const noexcept;

  std::string getLastFrameJson() const;

 private:
  void publish(StateEventKind kind, int pad, int slot, bool pressed);

  std::string buildFrameLocked(std::uint64_t sequence, StateEventKind kind,
                               int pad, int slot, bool pressed,
                               double timestampMs);

  StateStore& state;
  const StateBroadcastOptions options;

  std::atomic<bool> running{false};
  std::atomic<std::uint64_t> nextSequence{0};
  std::atomic<std::uint64_t> eventsPublished{0};
  std::atomic<std::uint64_t> framesSent{0};
  std::atomic<int> clients{0};

  mutable std::mutex frameMutex;
  std::array<PadMirror, StateBroadcastOptions::maxPads> pads{};
  std::string latestFrameJson;
  std::uint64_t latestSequence = 0;
};

}  // namespace Api
