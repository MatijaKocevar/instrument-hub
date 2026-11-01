#include "FreeTime.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runFreeTime()
{
    std::cout << "\n-- free (unsynced) time --" << std::endl;

    check (ihub::dsp::Delay::delaySamplesForMs (250.0f, 48000.0) == 12000,
           "250 ms at 48 kHz is 12000 samples");

    ihub::dsp::Delay delay;
    delay.prepareToPlay (48000.0, kBlockSize);

    ihub::dsp::DelaySettings settings;
    settings.tempoSync = false;
    settings.timeMs = 120.0f;
    settings.feedback = 0.0f;
    settings.mix = 1.0f;
    settings.dampingHz = 20000.0f;
    delay.setSettings (settings);

    const auto echo = measureEcho (delay, 32, 32 + 5760 + 256);

    std::cout << "      120 ms at 48 kHz: " << echo << " samples (expected 5760)"
              << std::endl;
    check (echo == 5760, "a 120 ms free delay renders at 5760 samples");
}

}
