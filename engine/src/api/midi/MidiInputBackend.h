#pragma once

#include <memory>
#include <string>
#include <vector>

namespace juce {
class MidiInputCallback;
}

namespace Api {

struct MidiInputDevice {
  std::string id;
  std::string name;
  std::string client;
};

class MidiInputHandle {
 public:
  virtual ~MidiInputHandle() = default;

  virtual void start() = 0;
  virtual void stop() = 0;
};

class MidiInputBackend {
 public:
  virtual ~MidiInputBackend() = default;

  virtual std::vector<MidiInputDevice> list() = 0;

  virtual std::unique_ptr<MidiInputHandle> open(const std::string& id) = 0;
};

std::unique_ptr<MidiInputBackend> makeJuceMidiInputBackend(
    juce::MidiInputCallback& callback);

std::vector<MidiInputDevice> listJuceMidiInputDevices();

}  // namespace Api
