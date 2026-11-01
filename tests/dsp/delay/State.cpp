#include "State.h"

#include "Fixtures.h"

namespace DspTest::Delay
{

void runState()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Delay source;
    source.prepareToPlay (48000.0, kBlockSize);

    ihub::dsp::DelaySettings settings;
    settings.tempoSync = false;
    settings.division = ihub::dsp::DelayDivision::dottedEighth;
    settings.timeMs = 333.0f;
    settings.feedback = 0.42f;
    settings.mix = 0.66f;
    settings.dampingHz = 4200.0f;
    source.setSettings (settings);
    source.setEnabled (false);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Delay restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    const auto restoredSettings = restored.getSettings();

    check (restoredSettings == settings, "settings round-trip");
    check (! restored.isEnabled(), "bypass round-trips");
}

}
