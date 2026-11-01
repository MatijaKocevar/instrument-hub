#include "Diagnostics.h"

namespace ihub::diag
{

namespace
{
/** Adapts the core Logger to the ring. It runs on the thread that logged, so
    it must obey the same allocation-free rules as LogRing::log(). */
class RingLogger final : public juce::Logger
{
public:
    void logMessage (const juce::String& message) override
    {
        defaultLog().log (LogLevel::info,
                          LogSource::general,
                          0,
                          message.toRawUTF8());
    }
};

} // namespace

void installJuceLoggerSink()
{
    static RingLogger logger;

    if (juce::Logger::getCurrentLogger() != &logger)
        juce::Logger::setCurrentLogger (&logger);
}

void uninstallJuceLoggerSink()
{
    // Leave another component's logger alone. RtGuard restores its own
    // previous logger when its strict-mode probe is removed.
    juce::Logger::setCurrentLogger (nullptr);
}

} // namespace ihub::diag
