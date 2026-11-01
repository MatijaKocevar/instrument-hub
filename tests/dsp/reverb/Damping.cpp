#include "Damping.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runDamping()
{
    std::cout << "\n-- damping --" << std::endl;

    ihub::dsp::Reverb bright;
    bright.prepareToPlay (kSampleRate, kBlockSize);
    bright.setSettings (makeSettings (0.7f, 0.0f, 0.0f, 1.0f));
    const auto brightResponse = renderImpulse (bright, 3.0);

    ihub::dsp::Reverb dark;
    dark.prepareToPlay (kSampleRate, kBlockSize);
    dark.setSettings (makeSettings (0.7f, 0.95f, 0.0f, 1.0f));
    const auto darkResponse = renderImpulse (dark, 3.0);

    const auto from = static_cast<int> (0.2 * kSampleRate);
    const auto to = static_cast<int> (0.8 * kSampleRate);

    const auto brightRatio = highFrequencyRatio (brightResponse, 0, from, to);
    const auto darkRatio = highFrequencyRatio (darkResponse, 0, from, to);

    std::cout << "      high-frequency ratio: bright " << brightRatio
              << ", damped " << darkRatio << std::endl;

    check (darkRatio < brightRatio,
           "heavy damping lowers the high-frequency content of the tail");
}

}
