#include "State.h"

#include "Fixtures.h"

namespace DspTest::Reverb
{

void runState()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Reverb source;
    source.prepareToPlay (kSampleRate, kBlockSize);

    const auto settings = makeSettings (0.77f, 0.22f, 0.33f, 0.44f, 55.0f);
    source.setSettings (settings);
    source.setEnabled (false);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Reverb restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (restored.getSettings() == settings, "settings round-trip");
    check (! restored.isEnabled(), "bypass round-trips");
}

}
