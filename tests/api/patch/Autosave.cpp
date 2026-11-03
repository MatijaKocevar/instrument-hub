#include "Autosave.h"

#include "../JsonSupport.h"
#include "../SessionRig.h"
#include "../TestSupport.h"
#include "../patch_state_capture_api/Fixtures.h"

#include "Session.h"
#include "app/project/ProjectAutosave.h"
#include "app/project/ProjectCapture.h"
#include "app/project/ProjectSession.h"
#include "project/Serializer.h"
#include "project/SetFile.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <string>
#include <vector>

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace ApiTest::PatchAutosave
{

namespace
{

constexpr int kPluginSlot = 0;
constexpr int kToneSlot = 1;
constexpr int kEmptySlot = 2;

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = 1;
    config.initialSlot = -1;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    config.slots.resize (3);

    auto& plugin = config.slots[static_cast<std::size_t> (kPluginSlot)];
    plugin.kind = ihub::SessionSlotKind::plugin;
    plugin.path = bundlePath;
    plugin.pluginFormat = "VST3";
    plugin.pluginName = "TestPlugin";
    plugin.instrumentKind = "plugin";
    plugin.name = "Saved Sound";
    plugin.patch = "saved-sound";
    plugin.patchName = plugin.name;

    auto& tone = config.slots[static_cast<std::size_t> (kToneSlot)];
    tone.kind = ihub::SessionSlotKind::initTone;
    tone.frequency = 220.0;
    tone.instrumentKind = "init-tone";
    tone.name = "Tone";
    tone.patch = "tone";
    tone.patchName = tone.name;

    config.slots[static_cast<std::size_t> (kEmptySlot)].kind = ihub::SessionSlotKind::empty;
    return config;
}

ihub::project::SlotConfig makeSlot (int index, const char* name,
                                    ihub::project::SourceKind kind)
{
    ihub::project::SlotConfig slot;
    slot.index = index;
    slot.name = name;
    slot.gain = 1.0f;
    slot.instrument.name = name;
    slot.instrument.kind = kind;
    slot.instrument.gain = 1.0f;
    return slot;
}

ihub::project::SetFile makeBaseSet (const juce::String& bundlePath)
{
    ihub::project::SetFile set;
    set.name = "Patch autosave";
    set.mappingYaml = juce::File (IHUB_SET_YAML).loadFileAsString().toStdString();

    auto plugin = makeSlot (kPluginSlot, "Saved Sound",
                            ihub::project::SourceKind::plugin);
    plugin.instrument.pluginFormat = "VST3";
    plugin.instrument.pluginName = "TestPlugin";
    plugin.instrument.pluginPath = bundlePath.toStdString();

    set.slots.push_back (std::move (plugin));
    set.slots.push_back (makeSlot (kToneSlot, "Tone",
                                   ihub::project::SourceKind::initTone));
    set.slots.push_back (makeSlot (kEmptySlot, "Empty",
                                   ihub::project::SourceKind::empty));
    return set;
}

void waitForDebounce()
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

}

bool runAutosave (const juce::File& root, const juce::String& bundlePath)
{
    const auto projectFile = root.getChildFile ("PatchAutosave.ihub");
    const auto stateDirectory = root.getChildFile ("patch-autosave-state");

    juce::String error;

    if (! ihub::project::Serializer::save (makeBaseSet (bundlePath), projectFile, error))
    {
        check (false, "the autosave base project saves: " + error.toStdString());
        return false;
    }

    SessionRig rig (makeConfig (bundlePath), 20, "patch-autosave-api", stateDirectory);

    if (! rig.started())
        return false;

    auto& client = rig.client();

    auto& state = rig.state();
    auto& session = rig.session();

    ihub::app::ProjectAutosave autosave (projectFile, 500);

    autosave.setCaptureHandler (
        [&session, &state, base = makeBaseSet (bundlePath)] (
            ihub::project::SetFile& set,
            const std::vector<int>& touchedSlots,
            juce::String& captureError)
        {
            return ihub::app::captureProject (session, state, base, touchedSlots,
                                              set, captureError);
        });

    autosave.setStateChangedHandler ([&state]
    {
        state.notifyProjectStateChanged();
    });

    state.setEditHandler ([&autosave] (const char* kind, int slot)
    {
        autosave.noteEdit (kind, slot);
    });

    state.setProjectStateProvider ([&autosave]
    {
        return autosave.getState();
    });

    auto response = client.Get ("/state");
    auto body = response ? parseJson (response->body) : juce::var {};

    check (response && response->status == 200,
           "GET /state before an edit returns 200");
    check (! boolProperty (property (body, "project"), "dirty"),
           "a loaded project starts clean");
    check (boolProperty (property (body, "project"), "saved"),
           "a loaded project starts saved");

    response = client.Patch (R"(/patches/1)",
                             R"({"volumeDb":-12.5,"pan":0.35,"colorIndex":7})",
                             "application/json");
    check (response && response->status == 200,
           "a patch mixer edit is accepted");

    body = response ? parseJson (response->body) : juce::var {};
    check (boolProperty (property (body, "project"), "dirty"),
           "the edit marks the project dirty");
    check (! boolProperty (property (body, "project"), "saved"),
           "the edit marks the project not saved");

    waitForDebounce();

    check (autosave.getSaveCount() == 1,
           "the patch edit produces one autosave write");
    check (! autosave.getState().dirty && autosave.getState().saved,
           "the autosave clears dirty after the write");

    ihub::project::SetFile saved;

    check (ihub::project::Serializer::load (projectFile, saved, error),
           "the autosaved patch edit reloads: " + error.toStdString());

    const auto* tone = findSlot (saved, kToneSlot);
    check (tone != nullptr
               && std::abs (tone->gain - linearGain (-12.5)) < 1.0e-4f
               && std::abs (tone->pan - 0.35f) < 1.0e-4f
               && tone->colorIndex == 7,
           "the patch mixer and colour reached the project file");

    ihub::SessionConfig restartedConfig;
    check (ihub::app::loadProjectSession (projectFile, restartedConfig, error),
           "a restart loads the autosaved project: " + error.toStdString());
    check (restartedConfig.slots.size() > static_cast<std::size_t> (kToneSlot)
               && std::abs (restartedConfig.slots[static_cast<std::size_t> (kToneSlot)].gain
                            - linearGain (-12.5)) < 1.0e-4f
               && std::abs (restartedConfig.slots[static_cast<std::size_t> (kToneSlot)].pan
                            - 0.35f) < 1.0e-4f
               && restartedConfig.slots[static_cast<std::size_t> (kToneSlot)].colorIndex == 7,
           "the restarted session sees the edited mixer and colour");

    response = client.Post (R"(/patches/1/fx)",
                            R"({"type":"eq","parameters":{"low":3.0,"high":-2.0}})",
                            "application/json");
    check (response && response->status == 200, "an FX chain edit is accepted");
    waitForDebounce();

    check (ihub::project::Serializer::load (projectFile, saved, error),
           "the autosaved FX chain reloads: " + error.toStdString());
    tone = findSlot (saved, kToneSlot);
    check (tone != nullptr && tone->fxChain.size() == 1
               && tone->fxChain.front().id == "eq",
           "the FX chain reached the project file");

    response = client.Post (R"(/patches/2/source)",
                            R"({"kind":"init-tone","name":"Tone Two"})",
                            "application/json");
    check (response && response->status == 200, "a source edit is accepted");
    waitForDebounce();

    check (ihub::project::Serializer::load (projectFile, saved, error),
           "the autosaved source reloads: " + error.toStdString());
    const auto* source = findSlot (saved, kEmptySlot);
    check (source != nullptr
               && source->instrument.kind == ihub::project::SourceKind::initTone,
           "the source change reached the project file");

    response = client.Post ("/transport", R"({"bpm":140})", "application/json");
    check (response && response->status == 200, "a transport edit is accepted");
    waitForDebounce();

    check (ihub::project::Serializer::load (projectFile, saved, error),
           "the autosaved transport reloads: " + error.toStdString());
    check (std::abs (saved.transport.bpm - 140.0) < 1.0e-4,
           "the transport tempo reached the project file");

    response = client.Post ("/arm", R"({"slot":0})", "application/json");
    check (response && response->status == 200, "the plugin patch arms");
    check (session.waitForSlotActive (kPluginSlot, 15000),
           "the plugin patch warms for the sound capture");

    PatchStateCaptureApi::applyPreset (session, kPluginSlot,
                                       PatchStateCaptureApi::kFrequencyA,
                                       PatchStateCaptureApi::kStateMarkerA);

    response = client.Patch (R"(/patches/0)", R"({"colorIndex":4})",
                             "application/json");
    check (response && response->status == 200,
           "an edit on the plugin patch is accepted");
    waitForDebounce();

    check (ihub::project::Serializer::load (projectFile, saved, error),
           "the autosaved plugin sound reloads: " + error.toStdString());

    const auto* plugin = findSlot (saved, kPluginSlot);
    check (plugin != nullptr && plugin->hasPluginState(),
           "the plugin patch owns a captured sound in the project");

    if (plugin != nullptr && plugin->hasPluginState())
    {
        const auto* chunk = saved.findPluginState (plugin->instrument.pluginState);
        check (chunk != nullptr && chunk->data.getSize() > 0,
               "the captured plugin sound has bytes");

        juce::MemoryBlock live;
        check (session.copyPluginStateData (kPluginSlot, live),
               "the running plugin still reports its sound");
        check (chunk != nullptr && chunk->data == live,
               "the project stores the sound captured before the write");
    }

    ihub::SessionConfig pluginRestart;
    check (ihub::app::loadProjectSession (projectFile, pluginRestart, error),
           "a restart loads the captured plugin sound: " + error.toStdString());
    check (pluginRestart.slots.size() > static_cast<std::size_t> (kPluginSlot)
               && pluginRestart.slots[static_cast<std::size_t> (kPluginSlot)]
                      .pluginState.getSize() > 0,
           "the restarted session carries the captured sound");

    const int savesBeforeSettings = autosave.getSaveCount();
    response = client.Post ("/settings", R"({"masterGain":0.75})",
                            "application/json");
    check (response && response->status == 200,
           "an app setting change is accepted");
    waitForDebounce();
    check (autosave.getSaveCount() == savesBeforeSettings,
           "restoring app settings never writes the project");

    rig.stop();
    return true;
}

}
