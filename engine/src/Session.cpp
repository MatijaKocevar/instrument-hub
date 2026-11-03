#include "Session.h"

#include "plugins/PluginInstrument.h"
#include "session/EditorHost.h"
#include "session/MirrorSync.h"

#include <cmath>

namespace ihub
{

namespace
{
constexpr double defaultFrequencies[] = {
    110.0, 123.47, 138.59, 146.83, 164.81, 185.0, 207.65, 220.0
};

constexpr const char* familyIds[] = {
    "keys", "drums", "bass", "pads", "lead", "fx", "perc", "vox"
};

constexpr const char* familyNames[] = {
    "Keys", "Drums", "Bass", "Pads", "Lead", "FX", "Percussion", "Vox"
};

constexpr int patchesPerFamily = 8;
}

const char* toString (SessionSlotKind kind) noexcept
{
    switch (kind)
    {
        case SessionSlotKind::empty:      return "empty";
        case SessionSlotKind::initTone:   return "init-tone";
        case SessionSlotKind::sampler:    return "sampler";
        case SessionSlotKind::plugin:     return "plugin";
        case SessionSlotKind::audioInput: return "audio-input";
    }

    return "empty";
}

const char* toString (SessionWarmState state) noexcept
{
    switch (state)
    {
        case SessionWarmState::cold:    return "cold";
        case SessionWarmState::warming: return "warming";
        case SessionWarmState::warm:    return "warm";
        case SessionWarmState::active:  return "active";
        case SessionWarmState::fading:  return "fading";
    }

    return "cold";
}

const char* toString (SessionLayout layout) noexcept
{
    switch (layout)
    {
        case SessionLayout::families: return "families";
        case SessionLayout::tracks:   return "tracks";
    }

    return "families";
}

Session::Session()
{
    config.slots = makeDefaultSet();
    pluginMirrors.resize (defaultLogicalSlots);
    slotErrors.resize (defaultLogicalSlots);
    loadAbort = std::make_shared<std::atomic<bool>> (false);

    editorHost = std::make_unique<session::EditorHost> (
        defaultLogicalSlots,
        [this] (int slot, SessionSlotConfig& slotConfig)
        {
            const std::lock_guard<std::mutex> lock (sourceMutex);

            if (slot < 0 || slot >= static_cast<int> (sourceConfigs.size()))
                return false;

            slotConfig = sourceConfigs[static_cast<std::size_t> (slot)];
            return true;
        },
        [this] (int slot)
        {
            return pluginState.getInstrument (slot);
        },
        [this] (int slot)
        {
            (void) capturePluginState (slot);
            syncMirrorState (slot);
        });

    mirrorSync = std::make_unique<session::MirrorSync> (
        defaultLogicalSlots,
        [this] (int slot)
        {
            return sandboxForSlot (slot);
        });
}

Session::~Session()
{
    beginShutdown();
    closeDevice();

    if (editorHost != nullptr)
        editorHost->closeAll();

    if (mirrorSync != nullptr)
        mirrorSync->shutdown();

    releaseAllPluginMirrors();
    warmPool.shutdown();
    switcher.reset();
    engine.reset();
    fxChain.reset();
    warmPool.clearVoices();
    pluginFormats.reset();
    mirrorSync.reset();
    editorHost.reset();
}

std::vector<SessionSlotConfig> Session::makeDefaultSet (SessionSlotKind defaultKind)
{
    std::vector<SessionSlotConfig> slots;
    slots.reserve (static_cast<std::size_t> (defaultLogicalSlots));

    for (int index = 0; index < defaultLogicalSlots; ++index)
    {
        const int familyIndex = index / patchesPerFamily;
        const int patchIndex = index % patchesPerFamily;
        const double familyScale = std::pow (1.25, static_cast<double> (familyIndex));

        SessionSlotConfig slot;
        slot.name = "Init " + std::to_string (index + 1);
        slot.kind = defaultKind;
        slot.instrumentKind = toString (defaultKind);
        slot.frequency = defaultFrequencies[patchIndex] * familyScale;
        slot.gain = 1.0f;

        slot.family = familyIds[familyIndex];
        slot.familyName = familyNames[familyIndex];
        slot.patch = "init-" + std::to_string (index);
        slot.patchName = slot.name;

        slots.push_back (std::move (slot));
    }

    return slots;
}

int Session::getNumSlots() const noexcept
{
    return engine != nullptr ? engine->getNumSlots() : 0;
}

AudioEngine::AudioEngine& Session::getEngine() noexcept
{
    jassert (engine != nullptr);
    return *engine;
}

const AudioEngine::AudioEngine& Session::getEngine() const noexcept
{
    jassert (engine != nullptr);
    return *engine;
}

AudioEngine::SlotSwitcher& Session::getSwitcher() noexcept
{
    jassert (switcher != nullptr);
    return *switcher;
}

const AudioEngine::SlotSwitcher& Session::getSwitcher() const noexcept
{
    jassert (switcher != nullptr);
    return *switcher;
}

int Session::getSlotLatencySamples (int slot) const noexcept
{
    if (engine == nullptr || slot < 0 || slot >= engine->getNumSlots())
        return 0;

    return engine->getSlot (slot).latencySamples();
}

std::vector<SessionSlotConfig> Session::copySourceConfigs() const
{
    const std::lock_guard<std::mutex> lock (sourceMutex);
    return sourceConfigs;
}

float Session::getSlotMeterPeak (int slot) const noexcept
{
    return meters.getSlotPeak (slot);
}

float Session::getMasterMeterPeak() const noexcept
{
    return meters.getMasterPeak();
}

int Session::getNeighbourCount() const noexcept
{
    return config.neighbourCount;
}

}
