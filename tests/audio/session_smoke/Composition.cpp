#include "Composition.h"

#include "Fixtures.h"

namespace AudioTest::SessionSmoke
{

void runComposition()
{
    ihub::Session session;
    juce::String error;

    const bool built = session.build (makeConfig (3.0f), error);

    check (built, "the default session builds" + (built ? std::string() : ": " + error.toStdString()));
    check (session.isPrepared(), "the session is prepared");
    check (session.getNumSlots() == kNumSlots, "64 slots were created");
    check (static_cast<int> (session.getSlotConfigs().size()) == kNumSlots,
           "the default set describes 64 slots");
    check (session.getSlotConfigs()[0].kind == ihub::SessionSlotKind::initTone,
           "the default slots are initialisation tones");
    check (session.getEngine().getBus().getNumChannels() == 2,
           "the summing bus is stereo");
    check (session.getEngine().getBus().getMaximumBlockSize() == kBlockSize,
           "the bus is sized at the maximum block size");
}

}
