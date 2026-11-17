#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace AudioEngine::detail {

class PipeWireQuantum {
 public:
  PipeWireQuantum() = default;
  ~PipeWireQuantum();

  PipeWireQuantum(const PipeWireQuantum&) = delete;
  PipeWireQuantum& operator=(const PipeWireQuantum&) = delete;

  bool apply(int frames, juce::String* errorMessage = nullptr);
  void restore();

 private:
  bool setFrames(int frames);

  bool applied = false;
  bool captured = false;
  int appliedFrames = 0;
  std::optional<int> previous;
};

}  // namespace AudioEngine::detail
