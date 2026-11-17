#include "Modes.h"

namespace ihub::app {

AppMode selectMode(const juce::String& commandLine) noexcept {
  if (commandLine.contains("--scan-plugins")) return AppMode::pluginScan;

  if (commandLine.contains("--list-devices")) return AppMode::listDevices;

  if (commandLine.contains("--measure-latency")) return AppMode::measureLatency;

  if (commandLine.contains("--diagnostics")) return AppMode::diagnostics;

  if (commandLine.contains("--serve")) return AppMode::serve;

  return AppMode::gui;
}

const char* toString(AppMode mode) noexcept {
  switch (mode) {
    case AppMode::gui:
      return "gui";
    case AppMode::serve:
      return "serve";
    case AppMode::pluginScan:
      return "scan-plugins";
    case AppMode::listDevices:
      return "list-devices";
    case AppMode::measureLatency:
      return "measure-latency";
    case AppMode::diagnostics:
      return "diagnostics";
  }

  return "unknown";
}

}  // namespace ihub::app
