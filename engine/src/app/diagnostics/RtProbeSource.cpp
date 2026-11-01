#include "app/diagnostics/RtProbeSource.h"

#include <cmath>
#include <mutex>

namespace ihub::app::detail
{

namespace
{

/** Keeps a deliberately injected allocation observable to the optimiser.

    `void* volatile` is a volatile pointer, so assigning to it is an observable
    side effect and the allocation cannot be elided at -O2.
*/
void* volatile allocationEscapeSink = nullptr;

} // namespace

const char* toString (RtInjection injection) noexcept
{
    switch (injection)
    {
        case RtInjection::none:       return "none";
        case RtInjection::allocation: return "allocation";
        case RtInjection::lock:       return "lock";
        case RtInjection::log:        return "log";
    }

    return "none";
}

//==============================================================================
RtProbeSource::RtProbeSource (RtInjection injectionToUse)
    : injection (injectionToUse),
      logText ("rt-guard injected log message")
{
}

void RtProbeSource::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    position = 0;
}

void RtProbeSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int samples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    const double phase = juce::MathConstants<double>::twoPi * frequency
                         * static_cast<double> (position) / sampleRate;
    const double increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;

    for (int i = 0; i < samples; ++i)
    {
        const auto value = static_cast<float> (amplitude
                                               * std::sin (phase + increment
                                                           * static_cast<double> (i)));

        for (int channel = 0; channel < channels; ++channel)
            buffer.getWritePointer (channel)[i] = value;
    }

    position += samples;

    if (! armed)
        return;

    switch (injection)
    {
        case RtInjection::none:
            break;

        case RtInjection::allocation:
        {
            auto* scratch = new float[8];
            scratch[0] = static_cast<float> (position);

            // Publish the pointer through a volatile sink so the optimiser
            // cannot reduce new/delete to a stack buffer at -O2.
            allocationEscapeSink = scratch;
            delete[] scratch;
            allocationEscapeSink = nullptr;
            break;
        }

        case RtInjection::lock:
        {
            std::mutex mutex;
            const std::lock_guard<std::mutex> lock (mutex);
            break;
        }

        case RtInjection::log:
            juce::Logger::writeToLog (logText);
            break;
    }
}

} // namespace ihub::app::detail
