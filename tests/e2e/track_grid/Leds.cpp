#include "track_grid/Leds.h"

#include "track_grid/Constants.h"

#include "app/surface/TrackSurface.h"
#include "controller/LaunchpadProtocol.h"

namespace TrackGridE2E
{

void attachLedStream (Controller::LaunchpadDriver& driver,
                      std::vector<LedEvent>& events)
{
    driver.setMessageSink ([&events] (const juce::MidiMessage& message)
    {
        if (! message.isNoteOn())
            return;

        const int pad = Launchpad::padIndexForNote (message.getNoteNumber());

        if (pad < 0)
            return;

        events.push_back ({ pad,
                            static_cast<std::uint8_t> (message.getVelocity()),
                            message.getChannel() - 1 });
    });
}

void paintTrackLeds (Api::StateStore& state,
                     const Controller::Mapping& mapping,
                     Controller::LaunchpadDriver& driver)
{
    const auto document = mapping.getMapping();

    if (document == nullptr)
        return;

    const auto snapshot = state.snapshot();
    ihub::app::paintTrackLeds (driver, *document,
                               ihub::app::trackLedSnapshotFor (snapshot));
}

int countRedTrackLeds (Controller::LaunchpadDriver& driver)
{
    int count = 0;

    for (int track = 0; track < 8; ++track)
        if (ledIs (driver, trackPad (track), Launchpad::Colour::Red))
            ++count;

    return count;
}

bool ledIs (Controller::LaunchpadDriver& driver,
            int pad,
            Launchpad::Colour colour)
{
    const auto led = driver.getPadLed (pad);
    return led.velocity == Launchpad::velocityFor (colour)
           && led.mode == Launchpad::LedMode::Static;
}

}
