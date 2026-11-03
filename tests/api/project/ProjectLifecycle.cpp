#include "ProjectLifecycle.h"

#include "ProjectFixtures.h"

#include "../JsonSupport.h"
#include "../SessionRig.h"
#include "../TestSupport.h"

#include "Session.h"
#include "app/project/ProjectLifecycle.h"
#include "app/project/ProjectPaths.h"
#include "app/project/ProjectSession.h"
#include "project/Serializer.h"
#include "settings/ProjectHistory.h"
#include "settings/SettingsStore.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace ApiTest::ProjectLifecycle
{

namespace
{

constexpr int kSlot = 0;

void writeText (const juce::File& file, const std::string& text)
{
    file.getParentDirectory().createDirectory();
    (void) file.replaceWithText (juce::String (text));
}

std::string projectBody (const char* action, const juce::File& file)
{
    auto body = juce::var (new juce::DynamicObject());
    body.getDynamicObject()->setProperty ("action", action);
    body.getDynamicObject()->setProperty ("path", file.getFullPathName());
    return juce::JSON::toString (body, false).toStdString();
}

void waitForAutosave()
{
    juce::Thread::sleep (900);
    juce::Timer::callPendingTimersSynchronously();
}

float linearGain (double decibels)
{
    return decibels <= -60.0 ? 0.0f
                             : static_cast<float> (std::pow (10.0, decibels / 20.0));
}

const ihub::project::SlotConfig* findSlot (const ihub::project::SetFile& set, int index)
{
    for (const auto& slot : set.slots)
        if (slot.index == index)
            return &slot;

    return nullptr;
}

juce::var familyWithId (const juce::var& state, const char* id)
{
    const auto* families = arrayProperty (state, "families");

    if (families == nullptr)
        return {};

    for (const auto& family : *families)
        if (stringProperty (family, "id") == id)
            return family;

    return {};
}

}

bool runLifecycle (const juce::File& root)
{
    const auto yamlA = root.getChildFile ("alpha.yaml");
    const auto yamlB = root.getChildFile ("beta.yaml");
    const auto projectA = root.getChildFile ("Alpha.ihub");
    const auto projectB = root.getChildFile ("Beta.ihub");

    const auto templateFile = juce::File (IHUB_SET_YAML);
    check (templateFile.existsAsFile(), "the bundled mapping template exists");

    const auto yamlAText = ProjectFixture::mappingYamlFromTemplate (templateFile,
                                                                    "alpha", "Alpha");
    const auto yamlBText = ProjectFixture::mappingYamlFromTemplate (templateFile,
                                                                    "beta", "Beta");

    writeText (yamlA, yamlAText);
    writeText (yamlB, yamlBText);

    setenv ("INSTRUMENT_HUB_SET", yamlA.getFullPathName().toRawUTF8(), 1);

    juce::String error;

    check (ProjectFixture::writeSet (
               projectA,
               ProjectFixture::makeSet ("Alpha Set", yamlAText, kSlot,
                                        "Alpha Sound", 0.5f, -0.2f, 3),
               error),
           "the alpha project saves: " + error.toStdString());

    check (ProjectFixture::writeSet (
               projectB,
               ProjectFixture::makeSet ("Beta Set", yamlBText, kSlot,
                                        "Beta Sound", 0.25f, 0.4f, 6),
               error),
           "the beta project saves: " + error.toStdString());

    ihub::SessionConfig config;
    check (ihub::app::loadProjectSession (projectA, config, error),
           "the alpha project produces a session: " + error.toStdString());

    ihub::SessionConfig betaConfig;
    check (ihub::app::loadProjectSession (projectB, betaConfig, error),
           "the beta project produces a session: " + error.toStdString());

    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;

    SessionRig rig (config, 20, "project-lifecycle-api");
    check (rig.started(), "the lifecycle rig starts");

    if (! rig.started())
        return false;

    const auto settingsFile = root.getChildFile ("settings.json");
    ihub::settings::SettingsStore settings (settingsFile);
    check (settings.load() == ihub::settings::LoadStatus::missing,
           "the lifecycle test starts from a missing settings file");

    ihub::app::ProjectLifecycle lifecycle (rig.session(), rig.state(), rig.server(),
                                            &settings);

    rig.server().stop();
    check (rig.server().start(),
           "the project lifecycle is bound into a fresh listener");

    check (lifecycle.attach (projectA, true, error),
           "the alpha project attaches to the lifecycle: " + error.toStdString());

    auto& client = rig.client();
    auto response = client.Get ("/project");
    auto body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "GET /project returns 200");
    check (stringProperty (body, "path") == projectA.getFullPathName().toStdString(),
           "GET /project reports the open project path");
    check (stringProperty (body, "name") == "Alpha",
           "GET /project reports the project name");
    check (! boolProperty (body, "dirty") && boolProperty (body, "saved"),
           "a freshly attached project is clean and saved");

    const auto* recent = arrayProperty (body, "recent");
    check (recent != nullptr && recent->size() == 1
               && stringProperty (recent->getFirst(), "path")
                      == projectA.getFullPathName().toStdString(),
           "the recent list starts with the attached project");

    response = client.Patch (R"(/patches/0)",
                              R"({"volumeDb":-12.5,"pan":0.4,"colorIndex":9})",
                              "application/json");
    body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "a mixer edit is accepted");
    check (boolProperty (property (body, "project"), "dirty"),
           "the mixer edit marks the project dirty");
    check (! boolProperty (property (body, "project"), "saved"),
           "the mixer edit marks the project not saved");

    waitForAutosave();

    response = client.Get ("/project");
    body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "GET /project returns 200 after autosave");
    check (! boolProperty (body, "dirty") && boolProperty (body, "saved"),
           "the autosave clears the dirty marker");

    ihub::project::SetFile saved;
    check (ihub::project::Serializer::load (projectA, saved, error),
           "the autosaved alpha project reloads: " + error.toStdString());

    const auto* slot = findSlot (saved, kSlot);
    check (slot != nullptr
               && std::abs (slot->gain - linearGain (-12.5)) < 1.0e-4f
               && std::abs (slot->pan - 0.4f) < 1.0e-4f
               && slot->colorIndex == 9,
           "the mixer edit reached the project file");

    const auto alphaBytes = ProjectFixture::readBytes (projectA);
    const auto copy = root.getChildFile ("Copy.ihub");

    response = client.Post ("/project", projectBody ("save-as", copy),
                            "application/json");
    body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "POST /project save-as succeeds");
    check (stringProperty (body, "path") == copy.getFullPathName().toStdString(),
           "save-as switches to the new project path");
    check (stringProperty (body, "name") == "Copy",
           "save-as derives the name from the new file");
    check (! boolProperty (body, "dirty") && boolProperty (body, "saved"),
           "the new project starts clean and saved");
    check (copy.existsAsFile(), "save-as writes the new project file");
    check (ProjectFixture::readBytes (projectA) == alphaBytes,
           "save-as leaves the original project byte-identical");

    ihub::project::SetFile copied;
    check (ihub::project::Serializer::load (copy, copied, error),
           "the save-as project reloads: " + error.toStdString());
    const auto* copiedSlot = findSlot (copied, kSlot);
    check (copiedSlot != nullptr
               && std::abs (copiedSlot->gain - linearGain (-12.5)) < 1.0e-4f
               && copiedSlot->colorIndex == 9,
           "the save-as project carries the running edit");

    response = client.Post ("/project", projectBody ("open", projectB),
                            "application/json");
    body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200,
           "POST /project open succeeds: "
               + (response ? response->body : std::string ("no response")));
    check (stringProperty (body, "path") == projectB.getFullPathName().toStdString(),
           "the open project path follows the request");
    check (! boolProperty (body, "dirty") && boolProperty (body, "saved"),
           "an opened project starts clean and saved");

    response = client.Get ("/state");
    auto state = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "GET /state returns 200 after open");
    check (! familyWithId (state, "alpha").isObject(),
           "the previous project's grid family is gone");
    check (familyWithId (state, "beta").isObject(),
           "the opened project's grid family is live");

    const auto patch = patchForSlot (state, kSlot);
    check (stringProperty (patch, "name") == "Grand Piano",
           "the grid follows the opened project");
    check (std::abs (numberProperty (patch, "volumeDb") - (-12.0412)) < 0.01
               && std::abs (numberProperty (patch, "pan") - 0.4) < 1.0e-4
               && intProperty (patch, "colorIndex") == 6,
           "the mixer follows the opened project");
    check (stringProperty (property (patch, "instrument"), "name") == "Beta Sound",
           "the instrument follows the opened project");

    const auto& configs = rig.session().getSlotConfigs();
    check (configs.size() == 64
               && configs[static_cast<std::size_t> (kSlot)].kind
                      == ihub::SessionSlotKind::initTone
               && configs[static_cast<std::size_t> (kSlot)].name == "Beta Sound",
           "the audio session rebuilt from the opened project");

    response = client.Post ("/project", projectBody ("open", projectA),
                            "application/json");
    check (response && response->status == 200, "the alpha project reopens");
    response = client.Get ("/state");
    state = response ? parseJson (response->body) : juce::var {};
    check (familyWithId (state, "alpha").isObject(),
           "reopening alpha restores its grid family");
    check (std::abs (numberProperty (patchForSlot (state, kSlot), "volumeDb")
                     - (-12.5)) < 0.01,
           "reopening alpha restores its autosaved mixer");

    setenv ("INSTRUMENT_HUB_SET", IHUB_SET_YAML, 1);

    const auto fresh = root.getChildFile ("Fresh.ihub");
    response = client.Post ("/project", projectBody ("new", fresh),
                            "application/json");
    body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200, "POST /project new succeeds");
    check (fresh.existsAsFile(), "new writes a fresh project file");
    check (stringProperty (body, "path") == fresh.getFullPathName().toStdString(),
           "new switches to the new project path");

    check (rig.session().getNumSlots() == 8,
           "the new project opens the eight-track session");
    check (rig.session().getLayout() == ihub::SessionLayout::tracks,
           "the new session reports the track layout");
    check (rig.session().getConfig().warmSlots
               == std::vector<int> { 0, 1, 2, 3, 4, 5, 6, 7 },
           "the new session protects all eight warm slots");

    check (rig.session().waitForWarmIdle (2000),
           "the eight track slots finish warming");

    response = client.Get ("/state");
    state = response ? parseJson (response->body) : juce::var {};

    check (stringProperty (state, "layout") == "tracks",
           "GET /state reports the track layout");
    check (! familyWithId (state, "alpha").isObject(),
           "the new project no longer carries the previous grid family");
    check (familyWithId (state, "track-1").isObject(),
           "the new project carries its first track");

    const auto* freshSlots = arrayProperty (state, "slots");
    check (freshSlots != nullptr && freshSlots->size() == 8,
           "GET /state lists eight track slots");

    int warmSlots = 0;

    if (freshSlots != nullptr)
        for (const auto& entry : *freshSlots)
            if (boolProperty (entry, "warm"))
                ++warmSlots;

    check (warmSlots == 8,
           "GET /state reports eight warm track slots (got "
               + std::to_string (warmSlots) + ")");

    response = client.Post ("/project", R"({"action":"close"})",
                            "application/json");
    body = response ? parseJson (response->body) : juce::var {};
    check (response && response->status == 200, "POST /project close succeeds");
    check (stringProperty (body, "path").empty(), "close reports no open project");
    check (! boolProperty (body, "dirty"), "close reports a clean lifecycle");

    (void) client.Patch (R"(/patches/0)", R"({"volumeDb":-3.0})",
                          "application/json");
    body = client.Get ("/project") ? parseJson (client.Get ("/project")->body)
                                   : juce::var {};
    check (stringProperty (body, "path").empty(),
           "an edit after close does not reopen the project");
    check (! boolProperty (body, "dirty"),
           "an edit after close does not mark a project dirty");

    const auto duplicate = client.Post ("/project", projectBody ("open", projectA),
                                         "application/json");
    check (duplicate && duplicate->status == 200,
           "a closed lifecycle can open a project again");

    rig.stop();
    return true;
}

}
