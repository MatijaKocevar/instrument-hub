#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <vector>

namespace ihub::plugins {

namespace scanner_cli {
inline constexpr const char* helperName = "ihub-scanner";
inline constexpr const char* scanFlag = "--scan";
inline constexpr const char* outFlag = "--out";
inline constexpr const char* formatFlag = "--format";
inline constexpr const char* defaultFormat = "VST3";
}  // namespace scanner_cli

struct ScanResult {
  juce::File file;
  bool succeeded = false;
  bool launched = false;
  bool timedOut = false;
  bool crashed = false;
  int exitCode = 0;
  double elapsedMs = 0.0;
  juce::String error;
  juce::String output;
  std::vector<juce::PluginDescription> plugins;
};

struct ScannerProcessOptions {
  juce::File helperExecutable;

  int timeoutMs = 20000;

  juce::String formatName{scanner_cli::defaultFormat};
};

class ScannerProcess {
 public:
  explicit ScannerProcess(ScannerProcessOptions options = {});

  ScanResult scanFile(const juce::File& pluginBundle);

  const ScannerProcessOptions& getOptions() const noexcept { return options; }

  static juce::File findHelperExecutable();

  juce::File getHelperExecutable() const noexcept {
    return options.helperExecutable;
  }

 private:
  ScannerProcessOptions options;
};
}  // namespace ihub::plugins
