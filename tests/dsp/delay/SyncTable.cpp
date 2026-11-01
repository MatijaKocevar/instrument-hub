#include "SyncTable.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runSyncTable()
{
    std::cout << "\n-- sync divisions and sample rates --" << std::endl;

    using D = ihub::dsp::DelayDivision;

    struct Case
    {
        D division;
        double bpm;
        double sampleRate;
        int expected;
    };

    const Case cases[] {
        { D::quarter,        120.0, 48000.0, 12000 },
        { D::quarter,         90.0, 48000.0, 16000 },
        { D::quarter,         60.0, 48000.0, 24000 },
        { D::quarter,        120.0, 44100.0, 11025 },
        { D::eighth,         120.0, 48000.0,  6000 },
        { D::half,           120.0, 48000.0, 24000 },
        { D::whole,          120.0, 48000.0, 48000 },
        { D::sixteenth,      120.0, 48000.0,  3000 },
        { D::dottedQuarter,  120.0, 48000.0, 18000 },
        { D::dottedEighth,   120.0, 48000.0,  9000 },
        { D::tripletQuarter, 120.0, 48000.0,  8000 },
        { D::tripletEighth,  120.0, 48000.0,  4000 },
    };

    for (const auto& testCase : cases)
    {
        const auto measured = ihub::dsp::Delay::delaySamplesForDivision (
            testCase.division, testCase.bpm, testCase.sampleRate);

        const auto label = "division " + std::to_string (static_cast<int> (testCase.division))
                           + " at " + number (testCase.bpm) + " BPM, "
                           + std::to_string (static_cast<int> (testCase.sampleRate)) + " Hz";

        check (measured == testCase.expected,
               label + " is " + std::to_string (testCase.expected)
                   + " samples (got " + std::to_string (measured) + ")");
    }

    ihub::dsp::Delay delay;
    delay.prepareToPlay (44100.0, kBlockSize);
    configure (delay, D::quarter, 120.0);

    const auto echo = measureEcho (delay, 64, 64 + 11025 + 256);
    std::cout << "      rendered 1/4 at 120 BPM / 44.1 kHz: " << echo << " samples"
              << std::endl;
    check (echo == 11025, "a rendered echo at 44.1 kHz is 11025 samples away");
}

}
