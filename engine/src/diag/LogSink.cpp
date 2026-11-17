#include "Diagnostics.h"

namespace ihub::diag {

namespace {
class RingLogger final : public juce::Logger {
 public:
  void logMessage(const juce::String& message) override {
    defaultLog().log(LogLevel::info, LogSource::general, 0,
                     message.toRawUTF8());
  }
};

}  // namespace

void installJuceLoggerSink() {
  static RingLogger logger;

  if (juce::Logger::getCurrentLogger() != &logger)
    juce::Logger::setCurrentLogger(&logger);
}

void uninstallJuceLoggerSink() { juce::Logger::setCurrentLogger(nullptr); }

}  // namespace ihub::diag
