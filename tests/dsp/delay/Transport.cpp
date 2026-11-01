#include "Transport.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runTransport()
{
    std::cout << "\n-- transport BPM change --" << std::endl;

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);
    configure (delay, ihub::dsp::DelayDivision::quarter, 120.0);

    // Let the delay run for a while, then change the tempo. The change is
    // adopted at the next block and the read tap crossfades to the new time.
    juce::AudioBuffer<float> warmUp (1, 2000);
    warmUp.clear();
    processInBlocks (delay, warmUp);

    ihub::dsp::DelayTransport transport;
    transport.playing = true;
    transport.bpm = 60.0;
    delay.setTransport (transport);

    check (delay.getDelaySamples() == 24000,
           "a 60 BPM quarter note reports 24000 samples");

    juce::AudioBuffer<float> settle (1, 4000);
    settle.clear();
    processInBlocks (delay, settle);

    const auto echo = measureEcho (delay, 0, 24000 + 512);

    std::cout << "      after changing to 60 BPM the echo is " << echo
              << " samples away" << std::endl;
    check (echo == 24000, "the impulse after the change repeats at 24000 samples");

    // The transport's PPQ position is accepted and used for the reported
    // musical position of the next repeat.
    transport.ppq = 3.0;
    delay.setTransport (transport);

    check (juce::exactlyEqual (delay.getTransport().ppq, 3.0),
           "the transport PPQ is stored");
    check (std::abs (delay.getDelayInPpq() - 0.5) < 1.0e-12,
           "at 60 BPM a 24000-sample delay is 0.5 PPQ (half a quarter note)");
    check (std::abs (delay.getNextRepeatPpq() - 3.5) < 1.0e-12,
           "the next repeat lands at PPQ 3.5 from PPQ 3.0");
}

} // namespace DspTest::Delay
