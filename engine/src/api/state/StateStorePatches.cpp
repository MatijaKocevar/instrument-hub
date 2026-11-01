#include "api/StateStore.h"

#include "api/Routes.h"

#include <cmath>
#include <utility>

namespace Api
{

namespace
{
float patchGainFromDb (double volumeDb) noexcept
{
    if (volumeDb <= -60.0)
        return 0.0f;

    return static_cast<float> (std::pow (10.0, volumeDb / 20.0));
}
} // namespace

//==============================================================================
// Patch mix / colour (T120)

PatchState StateStore::makePatchLocked (int slot) const
{
    PatchState patch;
    patch.slot = slot;

    if (slot < 0 || slot >= numSlots)
        return patch;

    const auto index = static_cast<std::size_t> (slot);
    patch.id = slotPatches[index];
    patch.name = slotPatchNames[index].empty() ? slotNames[index]
                                               : slotPatchNames[index];
    patch.family = slotFamilies[index];
    patch.familyName = slotFamilyNames[index];
    patch.instrument = slotInstruments[index];

    if (patch.instrument.name.empty())
        patch.instrument.name = patch.name;

    const auto& mix = slotMixes[index];
    patch.volumeDb = mix.volumeDb;
    patch.pan = mix.pan;
    patch.mute = mix.mute;
    patch.solo = mix.solo;
    patch.colorIndex = mix.colorIndex;
    patch.editorOpen = slotEditorOpen[index];

    return patch;
}

void StateStore::setPatchInstrument (int slot, PatchInstrumentState instrument)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (slot < 0 || slot >= numSlots)
        return;

    slotInstruments[static_cast<std::size_t> (slot)] = std::move (instrument);
}

void StateStore::setPatchMix (int slot,
                              double volumeDb,
                              double pan,
                              bool mute,
                              bool solo,
                              int colorIndex)
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (slot < 0 || slot >= numSlots)
        return;

    auto& mix = slotMixes[static_cast<std::size_t> (slot)];
    mix.volumeDb = juce::jlimit (-60.0, 12.0, volumeDb);
    mix.pan = juce::jlimit (-1.0, 1.0, pan);
    mix.mute = mute;
    mix.solo = solo;
    mix.colorIndex = juce::jlimit (0, 15, colorIndex);
}

PatchState StateStore::getPatch (int slot) const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return makePatchLocked (slot);
}

void StateStore::collectEffectiveGainsLocked (const std::vector<PatchMixState>& mixes,
                                              std::vector<float>& gains) const
{
    bool anySolo = false;

    for (const auto& mix : mixes)
        anySolo = anySolo || mix.solo;

    gains.assign (mixes.size(), 0.0f);

    for (std::size_t index = 0; index < mixes.size(); ++index)
    {
        const auto& mix = mixes[index];

        if (mix.mute || (anySolo && ! mix.solo))
            continue;

        gains[index] = juce::jlimit (0.0f, 4.0f,
                                     patchGainFromDb (mix.volumeDb));
    }
}

bool StateStore::updatePatch (int slot, const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    ChangeHandler changed;
    PatchHandler applyPatch;
    std::vector<PatchState> patches;
    std::vector<float> gains;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (slot < 0 || slot >= numSlots)
        {
            error = "slot is out of range";
            return false;
        }

        auto next = slotMixes[static_cast<std::size_t> (slot)];
        const auto& properties = patch.getDynamicObject()->getProperties();

        if (properties.isEmpty())
        {
            error = "body must contain at least one patch field";
            return false;
        }

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);
            const auto key = name.toString();

            if (key == "volumeDb")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "volumeDb", -60.0, 12.0,
                                          parsed, error))
                    return false;

                next.volumeDb = parsed;
            }
            else if (key == "pan")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "pan", -1.0, 1.0, parsed, error))
                    return false;

                next.pan = parsed;
            }
            else if (key == "mute" || key == "muted")
            {
                bool parsed = false;

                if (! detail::readBool (value, key.toRawUTF8(), parsed, error))
                    return false;

                next.mute = parsed;
            }
            else if (key == "solo")
            {
                bool parsed = false;

                if (! detail::readBool (value, "solo", parsed, error))
                    return false;

                next.solo = parsed;
            }
            else if (key == "colorIndex")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "colorIndex", 0, 15,
                                           parsed, error))
                    return false;

                next.colorIndex = static_cast<int> (parsed);
            }
            else
            {
                error = "unknown patch field: " + key.toStdString();
                return false;
            }
        }

        slotMixes[static_cast<std::size_t> (slot)] = next;

        collectEffectiveGainsLocked (slotMixes, gains);
        patches.reserve (static_cast<std::size_t> (numSlots));

        for (int index = 0; index < numSlots; ++index)
            patches.push_back (makePatchLocked (index));

        changed = changeHandler;
        applyPatch = patchHandler;
    }

    if (applyPatch != nullptr)
    {
        for (int index = 0; index < numSlots; ++index)
            applyPatch (index, patches[static_cast<std::size_t> (index)],
                        gains[static_cast<std::size_t> (index)]);
    }

    if (changed != nullptr)
        changed ("patch", slot);

    return true;
}

void StateStore::setPatchHandler (PatchHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchHandler = std::move (handler);
}


} // namespace Api
