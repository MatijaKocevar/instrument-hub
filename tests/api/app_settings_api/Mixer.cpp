#include "Mixer.h"

#include "Fixtures.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::AppSettings
{

void runMixerPersistence()
{
    Fixture fixture;
    const bool started = fixture.start();
    check (started, "the mixer persistence fixture starts");

    if (! started)
        return;

    auto& client = fixture.http();
    const auto noFile = ! fixture.file().existsAsFile();

    check (noFile, "the mixer test starts from a missing settings file");

    auto response = client.Post ("/settings", "{\"masterGain\":1.75}",
                                 "application/json");
    check (response && response->status == 400,
           "POST /settings rejects an out-of-range master gain");
    check (! fixture.file().existsAsFile(),
           "a rejected master gain writes no settings file");

    response = client.Post ("/settings", "{\"inputGain\":-0.1}",
                            "application/json");
    check (response && response->status == 400,
           "POST /settings rejects an out-of-range input gain");
    check (! fixture.file().existsAsFile(),
           "a rejected input gain writes no settings file");

    response = client.Post ("/settings", "{\"midiChannel\":0}",
                            "application/json");
    check (response && response->status == 400,
           "POST /settings rejects an out-of-range MIDI channel");
    check (! fixture.file().existsAsFile(),
           "a rejected MIDI channel writes no settings file");

    response = client.Post ("/settings", "{\"metronome\":\"yes\"}",
                            "application/json");
    check (response && response->status == 400,
           "POST /settings rejects a non-boolean metronome");
    check (! fixture.file().existsAsFile(),
           "a rejected metronome writes no settings file");

    response = client.Post ("/settings",
                            "{\"masterGain\":0.42,\"inputGain\":1.3,"
                            "\"midiChannel\":9,\"metronome\":true}",
                            "application/json");
    check (response && response->status == 200,
           "POST /settings accepts every app-level mixer value");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (std::abs (numberProperty (body, "masterGain") - 0.42) < 1.0e-9,
               "the response carries masterGain 0.42");
        check (std::abs (numberProperty (body, "inputGain") - 1.3) < 1.0e-9,
               "the response carries inputGain 1.3");
        check (intProperty (body, "midiChannel") == 9,
               "the response carries midiChannel 9");
        check (boolProperty (body, "metronome"),
               "the response carries metronome on");
    }

    check (fixture.file().existsAsFile(),
           "a successful POST /settings writes the settings file");
    check (isOwnerOnly (fixture.file()),
           "the settings file is readable by its owner only");

    const auto saved = fixture.file().loadFileAsString();
    const auto document = fixture.document();
    const auto mixer = property (document, "mixer");

    check (std::abs (numberProperty (mixer, "masterGain") - 0.42) < 1.0e-9
               && std::abs (numberProperty (mixer, "inputGain") - 1.3) < 1.0e-9
               && intProperty (mixer, "midiChannel") == 9
               && boolProperty (mixer, "metronome"),
           "the settings file stores every app-level mixer value");

    response = client.Post ("/settings", "{\"masterGain\":2.0}",
                            "application/json");
    check (response && response->status == 400,
           "POST /settings still rejects an out-of-range value after a save");
    check (fixture.file().loadFileAsString() == saved,
           "a rejected POST /settings leaves the settings file byte-identical");

    const auto held = fixture.state().getSettings();
    check (std::abs (held.masterGain - 0.42) < 1.0e-9
               && std::abs (held.inputGain - 1.3) < 1.0e-9
               && held.midiChannel == 9
               && held.metronome,
           "the live state holds the accepted values");

    check (fixture.restart(), "the state store and server restart");

    response = fixture.http().Get ("/settings");
    check (response && response->status == 200,
           "GET /settings answers after the restart");

    if (response && response->status == 200)
    {
        const auto body = parseJson (response->body);
        check (std::abs (numberProperty (body, "masterGain") - 0.42) < 1.0e-9,
               "masterGain returns after the restart");
        check (std::abs (numberProperty (body, "inputGain") - 1.3) < 1.0e-9,
               "inputGain returns after the restart");
        check (intProperty (body, "midiChannel") == 9,
               "midiChannel returns after the restart");
        check (boolProperty (body, "metronome"),
               "metronome returns after the restart");
    }
}

}
