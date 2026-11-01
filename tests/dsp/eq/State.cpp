#include "State.h"

#include "Fixtures.h"

namespace DspTest::Eq
{

void runState()
{
    std::cout << "\n-- state round-trip --" << std::endl;

    ihub::dsp::Eq source;
    source.prepareToPlay (kSampleRate, kBlockSize);

    const auto bands = makeTestBands();
    source.setAllBands (bands);
    source.setEnabled (false);

    juce::MemoryBlock state;
    source.getStateInformation (state);

    ihub::dsp::Eq destination;
    destination.prepareToPlay (kSampleRate, kBlockSize);
    destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (destination.getAllBands() == bands, "all three bands survive a round-trip");
    check (! destination.isEnabled(), "the global enable survives a round-trip");
}

}
