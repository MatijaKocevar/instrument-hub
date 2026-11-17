#include "SandboxHost.h"

#include "sandbox/SandboxWatchdog.h"

#include <cstdlib>

namespace ihub::plugins {
SandboxHost::SandboxHost() : SandboxHost(Options()) {}

SandboxHost::SandboxHost(Options optionsToUse)
    : options(std::move(optionsToUse)) {
  if (options.maxBlockSize <= 0) options.maxBlockSize = 512;

  if (options.numChannels <= 0) options.numChannels = 2;

  if (options.sampleRate <= 0.0) options.sampleRate = 48000.0;

  if (options.startupTimeoutMs <= 0) options.startupTimeoutMs = 15000;

  if (options.hangTimeoutMs <= 0) options.hangTimeoutMs = 5000;
}

SandboxHost::~SandboxHost() { stop(); }

juce::File SandboxHost::findHelperExecutable() {
  if (auto* overridden = std::getenv("IHUB_SANDBOX"))
    if (*overridden != 0)
      return juce::File(juce::String::fromUTF8(overridden)).getFullPathName();

#if JUCE_WINDOWS
  const auto suffix = juce::String(".exe");
#else
  const auto suffix = juce::String();
#endif

  const auto appDirectory =
      juce::File::getSpecialLocation(juce::File::currentExecutableFile)
          .getParentDirectory();
  const auto name = juce::String(sandbox::cli::helperName) + suffix;
  const auto direct = appDirectory.getChildFile(name);

  if (direct.existsAsFile()) return direct;

  const juce::Array<juce::File> candidates{
      appDirectory.getParentDirectory().getChildFile("engine").getChildFile(
          name),
      appDirectory.getParentDirectory()
          .getParentDirectory()
          .getChildFile("engine")
          .getChildFile(name)};

  for (const auto& candidate : candidates)
    if (candidate.existsAsFile()) return candidate;

  return direct;
}

const char* toString(SandboxHost::Status status) noexcept {
  switch (status) {
    case SandboxHost::Status::stopped:
      return "stopped";
    case SandboxHost::Status::starting:
      return "starting";
    case SandboxHost::Status::running:
      return "running";
    case SandboxHost::Status::failed:
      return "failed";
  }

  return "failed";
}

}  // namespace ihub::plugins
