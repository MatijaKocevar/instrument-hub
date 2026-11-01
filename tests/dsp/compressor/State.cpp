#include "State.h"

#include "Fixtures.h"

namespace DspTest::Compressor
{

void runState()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Compressor source;
    source.prepareToPlay (kSampleRate, kBlockSize);

    ihub::dsp::CompressorSettings settings;
    settings.thresholdDb = -24.5f;
    settings.ratio = 6.5f;
    settings.attackMs = 3.25f;
    settings.releaseMs = 250.0f;
    settings.makeupDb = 4.0f;

    source.setSettings (settings);
    source.setEnabled (false);
    source.setLookaheadMs (12.0f);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Compressor destination;
    destination.prepareToPlay (kSampleRate, kBlockSize);
    destination.setStateInformation (state.getData(),
                                     static_cast<int> (state.getSize()));

    check (destination.getSettings() == settings, "settings survive a round-trip");
    check (! destination.isEnabled(), "bypass survives a round-trip");
    check (std::abs (destination.getLookaheadMs() - 12.0f) <= 0.001f,
           "the lookahead setting survives a round-trip");
}

} // namespace DspTest::Compressor
