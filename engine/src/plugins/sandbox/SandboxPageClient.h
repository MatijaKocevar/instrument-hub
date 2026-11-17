#pragma once

#include <juce_core/juce_core.h>

namespace ihub::plugins {

class SandboxPageClient final {
 public:
  static int run(const juce::StringArray& commandLine);

 private:
  SandboxPageClient() = delete;
};

}  // namespace ihub::plugins
