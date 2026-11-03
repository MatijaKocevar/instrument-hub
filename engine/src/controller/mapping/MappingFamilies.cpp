#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>
#include <utility>

namespace Controller::MappingDetail
{

bool decodeFamilies (const YamlValue& root, SetMapping& out, MappingError& error)
{
    const auto* families = root.find ("families");
    const bool trackLayout = out.layout == MappingLayout::tracks;

    if (families == nullptr || (trackLayout && families->kind == YamlKind::nullValue))
    {
        if (trackLayout)
            return true;

        return fail (error, root, "families is required");
    }

    if (families->kind != YamlKind::mapping)
        return fail (error, *families, "families must be a mapping of family ids");

    if (families->mapping.empty())
    {
        if (trackLayout)
            return true;

        return fail (error, *families, "families must not be empty");
    }

    for (const auto& [familyId, familyValue] : families->mapping)
    {
        if (familyId.empty())
            return fail (error, familyValue, "a family id must not be empty");

        if (familyValue.kind != YamlKind::mapping)
            return fail (error, familyValue, "family '" + familyId + "' must be a mapping");

        if (! checkKeys (familyValue, { "name", "instruments", "colorIndex" }, error))
            return false;

        FamilySpec family;
        family.id = familyId;

        const auto* familyName = familyValue.find ("name");

        if (familyName == nullptr)
            return fail (error, familyValue, "family '" + familyId + "' needs a name");

        if (! readString (*familyName, family.name, error, "family name"))
            return false;

        if (const auto* familyColour = familyValue.find ("colorIndex"))
            if (! readColourIndex (*familyColour, family.colorIndex, error, "family colorIndex"))
                return false;

        const auto* instruments = familyValue.find ("instruments");

        if (instruments == nullptr || instruments->kind != YamlKind::sequence
            || instruments->sequence.empty())
            return fail (error, familyValue,
                         "family '" + familyId + "' needs a non-empty instruments list");

        for (const auto& instrumentValue : instruments->sequence)
        {
            if (instrumentValue.kind != YamlKind::mapping)
                return fail (error, instrumentValue,
                             "an instrument in family '" + familyId + "' must be a mapping");

            if (! checkKeys (instrumentValue,
                             { "id", "name", "slot", "colorIndex", "volumeDb", "pan",
                               "mute", "solo", "kind", "pluginFormat", "pluginId",
                               "pluginName", "pluginManufacturer", "pluginVersion",
                               "pluginState", "samplePath", "fx" },
                             error))
                return false;

            InstrumentSpec instrument;

            const auto* id = instrumentValue.find ("id");
            const auto* name = instrumentValue.find ("name");
            const auto* slot = instrumentValue.find ("slot");

            if (id == nullptr || name == nullptr || slot == nullptr)
                return fail (error, instrumentValue,
                             "an instrument needs id, name and slot");

            if (! readString (*id, instrument.id, error, "instrument id")
                || ! readString (*name, instrument.name, error, "instrument name", true)
                || ! readInt (*slot, instrument.slot, error, "instrument slot"))
                return false;

            if (instrument.slot < 0 || instrument.slot > SetMapping::maximumSlot)
                return fail (error, *slot,
                             "instrument slot must be within 0.."
                                 + std::to_string (SetMapping::maximumSlot));

            if (family.findInstrument (instrument.id) != nullptr)
                return fail (error, *id,
                             "duplicate instrument id '" + instrument.id
                                 + "' in family '" + familyId + "'");

            if (const auto* colour = instrumentValue.find ("colorIndex"))
                if (! readInt (*colour, instrument.colorIndex, error, "patch colorIndex"))
                    return false;

            if (instrument.colorIndex != InstrumentSpec::inheritColorIndex
                && (instrument.colorIndex < 0
                    || instrument.colorIndex > SetMapping::maximumColourIndex))
                return fail (error, *instrumentValue.find ("colorIndex"),
                             "patch colorIndex must be -1 (inherit) or within 0.."
                                 + std::to_string (SetMapping::maximumColourIndex));

            if (const auto* volume = instrumentValue.find ("volumeDb"))
            {
                if (! readFloat (*volume, instrument.volumeDb, error, "patch volumeDb"))
                    return false;

                if (instrument.volumeDb < -60.0f || instrument.volumeDb > 12.0f)
                    return fail (error, *volume, "patch volumeDb must be within -60..+12");
            }

            if (const auto* balance = instrumentValue.find ("pan"))
            {
                if (! readFloat (*balance, instrument.pan, error, "patch pan"))
                    return false;

                if (instrument.pan < -1.0f || instrument.pan > 1.0f)
                    return fail (error, *balance, "patch pan must be within -1..+1");
            }

            if (const auto* mute = instrumentValue.find ("mute"))
                if (! readBool (*mute, instrument.muted, error, "patch mute"))
                    return false;

            if (const auto* solo = instrumentValue.find ("solo"))
                if (! readBool (*solo, instrument.solo, error, "patch solo"))
                    return false;

            if (const auto* kindValue = instrumentValue.find ("kind"))
                if (! readString (*kindValue, instrument.kind, error, "patch kind"))
                    return false;

            if (instrument.kind != "empty" && instrument.kind != "init-tone"
                && instrument.kind != "sampler"
                && instrument.kind != "plugin" && instrument.kind != "audio-input")
                return fail (error, *instrumentValue.find ("kind"),
                             "patch kind must be empty, init-tone, sampler, plugin or audio-input");

            const auto readOptional =
                [&instrumentValue, &error] (const char* key, std::string& destination)
            {
                if (const auto* value = instrumentValue.find (key))
                    return readString (*value, destination, error, key, true);

                return true;
            };

            if (! readOptional ("pluginFormat", instrument.pluginFormat)
                || ! readOptional ("pluginId", instrument.pluginId)
                || ! readOptional ("pluginName", instrument.pluginName)
                || ! readOptional ("pluginManufacturer", instrument.pluginManufacturer)
                || ! readOptional ("pluginVersion", instrument.pluginVersion)
                || ! readOptional ("pluginState", instrument.pluginState)
                || ! readOptional ("samplePath", instrument.samplePath))
                return false;

            if (instrument.kind == "plugin" && instrument.pluginId.empty()
                && instrument.pluginName.empty())
                return fail (error, instrumentValue,
                             "a plugin patch needs a pluginId or pluginName descriptor");

            if (const auto* fxList = instrumentValue.find ("fx"))
            {
                if (fxList->kind != YamlKind::sequence)
                    return fail (error, *fxList, "patch fx must be a sequence");

                for (const auto& fxValue : fxList->sequence)
                {
                    PatchFxSpec fx;

                    if (! readFx (fxValue, fx, error))
                        return false;

                    instrument.fxChain.push_back (std::move (fx));
                }
            }

            instrument.resolvedColorIndex =
                instrument.colorIndex == InstrumentSpec::inheritColorIndex
                    ? family.colorIndex
                    : instrument.colorIndex;

            family.instruments.push_back (std::move (instrument));
        }

        out.families.emplace (family.id, std::move (family));
    }

    return true;
}

}
