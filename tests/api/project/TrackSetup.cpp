#include "TrackSetup.h"

#include "ProjectFixtures.h"

#include "../JsonSupport.h"
#include "../SessionRig.h"
#include "../TestSupport.h"

#include "Session.h"
#include "app/project/ProjectImport.h"
#include "app/project/ProjectLifecycle.h"
#include "app/project/ProjectSession.h"
#include "controller/MappingYaml.h"
#include "project/Serializer.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <string>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace ApiTest::ProjectLifecycle
{

namespace
{

constexpr int kTracks = 8;
constexpr const char* insertTypes[] = { "eq", "compressor", "delay", "reverb" };

int volumeStepFor (int track)
{
    return track % 5;
}

double gainForStep (int step)
{
    static constexpr double gains[] = { 0.0, 0.5, 0.75, 1.0,
                                        1.4125375446227544 };
    return gains[step];
}

std::string firstInsertId (int track)
{
    return "t" + std::to_string (track) + "-a";
}

std::string secondInsertId (int track)
{
    return "t" + std::to_string (track) + "-b";
}

bool hasSecondInsert (int track)
{
    return track % 2 == 1;
}

juce::var trackAt (const juce::var& state, int index)
{
    const auto* tracks = arrayProperty (state, "tracks");

    if (tracks == nullptr || index < 0 || index >= tracks->size())
        return {};

    return (*tracks)[index];
}

juce::var hotFxAt (const juce::var& track, int index)
{
    const auto* hot = arrayProperty (track, "hotFx");

    if (hot == nullptr || index < 0 || index >= hot->size())
        return {};

    return (*hot)[index];
}

juce::var insertAt (const juce::var& chain, int index)
{
    const auto* inserts = arrayProperty (chain, "inserts");

    if (inserts == nullptr || index < 0 || index >= inserts->size())
        return {};

    return (*inserts)[index];
}

void waitForAutosave()
{
    juce::Thread::sleep (900);
    juce::Timer::callPendingTimersSynchronously();
}

void setTrackSource (httplib::Client& client, int track)
{
    const auto body = "{\"kind\":\"init-tone\",\"name\":\"Tone "
                      + std::to_string (track) + "\",\"frequency\":"
                      + std::to_string (220 + track * 20) + "}";
    auto response = client.Post ("/patches/" + std::to_string (track) + "/source",
                                 body, "application/json");
    check (response && response->status == 200,
           "track " + std::to_string (track) + " chooses an instrument");
}

void addInsert (httplib::Client& client, int track, int slot,
                const std::string& id, const char* type)
{
    const auto body = std::string ("{\"type\":\"") + type
                      + "\",\"id\":\"" + id
                      + "\",\"name\":\"" + type + " "
                      + std::to_string (track) + "\"}";
    auto response = client.Post ("/patches/" + std::to_string (track) + "/fx",
                                 body, "application/json");
    check (response && response->status == 200,
           "track " + std::to_string (track) + " adds insert " + std::to_string (slot));
}

void assignHot (httplib::Client& client, int track, int hot, const std::string& id)
{
    auto response = client.Post ("/tracks/" + std::to_string (track) + "/fx/"
                                     + std::to_string (hot) + "/assign",
                                 "{\"insertId\":\"" + id + "\"}",
                                 "application/json");
    check (response && response->status == 200,
           "track " + std::to_string (track) + " assigns hot FX " + std::to_string (hot));
}

void setVolume (httplib::Client& client, int track)
{
    auto response = client.Post ("/tracks/" + std::to_string (track) + "/volume",
                                 "{\"step\":" + std::to_string (volumeStepFor (track)) + "}",
                                 "application/json");
    check (response && response->status == 200,
           "track " + std::to_string (track) + " sets its volume step");
}

void buildTrackSetup (httplib::Client& client)
{
    for (int track = 0; track < kTracks; ++track)
    {
        setTrackSource (client, track);
        addInsert (client, track, 0, firstInsertId (track),
                   insertTypes[track % 4]);

        if (hasSecondInsert (track))
            addInsert (client, track, 1, secondInsertId (track),
                       insertTypes[(track + 1) % 4]);

        if (track == 6)
        {
            assignHot (client, track, 1, firstInsertId (track));
        }
        else
        {
            assignHot (client, track, 0, firstInsertId (track));

            if (hasSecondInsert (track))
                assignHot (client, track, 1, secondInsertId (track));
        }

        setVolume (client, track);
    }

    auto response = client.Post ("/tracks/0/fx/0/toggle", "{}", "application/json");
    check (response && response->status == 200,
           "the first track bypasses its hot FX");
}

void checkSavedSet (const ihub::project::SetFile& set)
{
    check (set.slots.size() == kTracks,
           "the saved track project keeps eight slots");

    Controller::SetMapping mapping;
    Controller::MappingError mappingError;
    check (Controller::parseSetMappingYaml (set.mappingYaml, mapping, mappingError),
           "the saved track mapping parses: " + mappingError.message);

    for (int track = 0; track < kTracks; ++track)
    {
        const auto label = std::to_string (track);
        const ihub::project::SlotConfig* slot = nullptr;

        for (const auto& candidate : set.slots)
            if (candidate.index == track)
                slot = &candidate;

        check (slot != nullptr, "track slot " + label + " is in the project");

        if (slot == nullptr)
            continue;

        check (slot->instrument.kind == ihub::project::SourceKind::initTone
                   && slot->instrument.name == "Tone " + label,
               "track slot " + label + " keeps its instrument");
        check (std::abs (slot->gain - static_cast<float> (gainForStep (volumeStepFor (track))))
                   < 1.0e-4f,
               "track slot " + label + " keeps its volume");
        check (slot->fxChain.size() == (hasSecondInsert (track) ? 2u : 1u),
               "track slot " + label + " keeps its whole FX chain");
        check (! slot->fxChain.empty()
                   && slot->fxChain.front().id == firstInsertId (track),
               "track slot " + label + " keeps its first insert identity");

        if (hasSecondInsert (track))
            check (slot->fxChain.size() == 2
                       && slot->fxChain[1].id == secondInsertId (track),
                   "track slot " + label + " keeps its second insert identity");

        if (track == 0)
            check (! slot->fxChain.empty() && slot->fxChain.front().bypassed,
                   "the bypassed hot FX is saved in the chain");
    }

    for (const auto& entry : mapping.tracks)
    {
        const int track = std::stoi (entry.id.substr (6)) - 1;
        const auto label = std::to_string (track);

        if (track == 6)
        {
            check (entry.hotFx.size() == 2 && entry.hotFx[0].empty()
                       && entry.hotFx[1] == firstInsertId (track),
                   "track " + label + " keeps its second hot FX slot");
        }
        else
        {
            check (entry.hotFx.size() >= 1
                       && entry.hotFx[0] == firstInsertId (track),
                   "track " + label + " keeps its first hot FX slot");
        }
    }
}

void checkRestartedState (httplib::Client& client)
{
    auto response = client.Get ("/state");
    const auto state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200,
           "GET /state returns 200 after the track restart");
    check (stringProperty (state, "layout") == "tracks",
           "the restarted project reports the track layout");

    const auto* tracks = arrayProperty (state, "tracks");
    check (tracks != nullptr && tracks->size() == kTracks,
           "the restarted project lists eight tracks");

    for (int track = 0; track < kTracks; ++track)
    {
        const auto label = std::to_string (track);
        const auto value = trackAt (state, track);

        check (stringProperty (property (value, "instrument"), "name")
                   == "Tone " + label,
               "track " + label + " returns with its instrument");
        check (intProperty (value, "volumeStep") == volumeStepFor (track)
                   && std::abs (numberProperty (value, "gain")
                                - gainForStep (volumeStepFor (track))) < 1.0e-6,
               "track " + label + " returns with its volume step");

        if (track == 6)
        {
            check (stringProperty (hotFxAt (value, 0), "id").empty()
                       && stringProperty (hotFxAt (value, 1), "id")
                              == firstInsertId (track),
                   "track " + label + " returns its hot FX in the saved slot");
        }
        else
        {
            check (stringProperty (hotFxAt (value, 0), "id") == firstInsertId (track),
                   "track " + label + " returns its first hot FX");

            if (hasSecondInsert (track))
                check (stringProperty (hotFxAt (value, 1), "id")
                           == secondInsertId (track),
                       "track " + label + " returns its second hot FX");
        }

        response = client.Get ("/patches/" + label + "/fx");
        const auto chain = response ? parseJson (response->body) : juce::var {};

        check (response && response->status == 200,
               "track " + label + " still publishes its FX chain");
        check (stringProperty (insertAt (chain, 0), "id") == firstInsertId (track),
               "track " + label + " restores its first insert");

        if (hasSecondInsert (track))
            check (stringProperty (insertAt (chain, 1), "id")
                       == secondInsertId (track),
                   "track " + label + " restores its second insert");

        if (track == 0)
            check (boolProperty (insertAt (chain, 0), "bypassed"),
                   "the bypassed insert returns after the restart");
    }
}

void checkFailedWrite (httplib::Client& client, const juce::File& project)
{
    const auto before = ProjectFixture::readBytes (project);
    check (project.setReadOnly (true), "the track project can be marked read-only");

    auto response = client.Post ("/tracks/0/volume", "{\"step\":2}",
                                 "application/json");
    check (response && response->status == 200,
           "the read-only project still accepts a volume change");
    waitForAutosave();

    check (ProjectFixture::readBytes (project) == before,
           "a failed track autosave leaves the previous .ihub byte-identical");

    response = client.Post ("/project", R"({"action":"save"})",
                            "application/json");
    check (response && response->status == 400,
           "an explicit save reports the read-only failure");
    check (ProjectFixture::readBytes (project) == before,
           "the failed explicit save keeps the project byte-identical");

    check (project.setReadOnly (false), "the track project can be made writable again");
}

}

void runTrackSetup (const juce::File& root)
{
    const auto project = root.getChildFile ("Tracks.ihub");
    setenv ("INSTRUMENT_HUB_SET", IHUB_SET_YAML, 1);

    juce::String error;
    bool created = false;
    check (ihub::app::ensureProjectFile (project, error, &created),
           "the blank track project is created: " + error.toStdString());

    ihub::SessionConfig config;
    check (ihub::app::loadProjectSession (project, config, error),
           "the blank track project builds a session: " + error.toStdString());
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;

    {
        SessionRig rig (config, 20, "track-setup-save");
        check (rig.started(), "the track setup rig starts");

        if (! rig.started())
            return;

        ihub::app::ProjectLifecycle lifecycle (rig.session(), rig.state(),
                                                rig.server(), nullptr);
        rig.server().stop();
        check (rig.server().start(), "the track setup lifecycle is bound");
        check (lifecycle.attach (project, false, error),
               "the blank track project attaches: " + error.toStdString());

        buildTrackSetup (rig.client());
        waitForAutosave();

        auto response = rig.client().Get ("/project");
        const auto overview = response ? parseJson (response->body) : juce::var {};
        check (response && response->status == 200
                   && ! boolProperty (overview, "dirty")
                   && boolProperty (overview, "saved"),
               "the track setup autosaves cleanly");

        ihub::project::SetFile saved;
        check (ihub::project::Serializer::load (project, saved, error),
               "the autosaved track project reloads: " + error.toStdString());
        checkSavedSet (saved);
    }

    ihub::SessionConfig restartedConfig;
    check (ihub::app::loadProjectSession (project, restartedConfig, error),
           "the saved track project builds a restart session: "
               + error.toStdString());
    restartedConfig.device.sampleRate = 48000.0;
    restartedConfig.device.bufferSize = 128;
    restartedConfig.device.numOutputChannels = 2;
    restartedConfig.device.numInputChannels = 2;

    {
        SessionRig rig (restartedConfig, 20, "track-setup-restart");
        check (rig.started(), "the restart rig starts");

        if (! rig.started())
            return;

        ihub::app::ProjectLifecycle lifecycle (rig.session(), rig.state(),
                                                rig.server(), nullptr);
        rig.server().stop();
        check (rig.server().start(), "the restart lifecycle is bound");
        check (lifecycle.attach (project, false, error),
               "the saved track project attaches after restart: "
                   + error.toStdString());

        checkRestartedState (rig.client());
        checkFailedWrite (rig.client(), project);
    }

    setenv ("INSTRUMENT_HUB_SET", IHUB_SET_YAML, 1);
}

}
