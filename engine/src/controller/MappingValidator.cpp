#include "MappingYamlInternal.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace Controller::MappingDetail
{

namespace
{

bool fail (MappingError& error, const YamlValue& value, const std::string& message)
{
    error = { value.line, message };
    return false;
}

bool checkKeys (const YamlValue& value,
                std::initializer_list<std::string_view> allowed,
                MappingError& error)
{
    for (const auto& [key, child] : value.mapping)
    {
        (void) child;

        bool known = false;

        for (const auto entry : allowed)
            if (key == entry)
                known = true;

        if (! known)
            return fail (error, value, "unknown key '" + key + "'");
    }

    return true;
}

bool readString (const YamlValue& value, std::string& out, MappingError& error,
                 const std::string& what, bool allowEmpty = false)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be a string");

    if (! allowEmpty && value.scalar.empty())
        return fail (error, value, what + " must not be empty");

    out = value.scalar;
    return true;
}

bool readInt (const YamlValue& value, int& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be an integer");

    std::string text = value.scalar;

    if (! text.empty() && text.front() == '+')
        text.erase (text.begin());

    if (text.empty())
        return fail (error, value, what + " must be an integer");

    int parsed = 0;
    const auto result = std::from_chars (text.data(), text.data() + text.size(), parsed);

    if (result.ec != std::errc() || result.ptr != text.data() + text.size())
        return fail (error, value, what + " must be an integer");

    out = parsed;
    return true;
}

bool readFloat (const YamlValue& value, float& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be a number");

    if (value.scalar.empty())
        return fail (error, value, what + " must be a number");

    char* end = nullptr;
    const float parsed = std::strtof (value.scalar.c_str(), &end);

    if (end == value.scalar.c_str() || *end != '\0' || ! std::isfinite (parsed))
        return fail (error, value, what + " must be a finite number");

    out = parsed;
    return true;
}

bool readBool (const YamlValue& value, bool& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be true or false");

    if (value.scalar == "true")
    {
        out = true;
        return true;
    }

    if (value.scalar == "false")
    {
        out = false;
        return true;
    }

    return fail (error, value, what + " must be true or false");
}

bool readColourIndex (const YamlValue& value, int& out, MappingError& error,
                      const std::string& what)
{
    if (! readInt (value, out, error, what))
        return false;

    if (out < 0 || out > SetMapping::maximumColourIndex)
        return fail (error, value, what + " must be within 0.."
                         + std::to_string (SetMapping::maximumColourIndex));

    return true;
}

bool readFx (const YamlValue& value, PatchFxSpec& fx, MappingError& error)
{
    if (value.kind != YamlKind::mapping)
        return fail (error, value, "an fx entry must be a mapping");

    if (! checkKeys (value, { "kind", "id", "name", "pluginState", "bypassed",
                              "parameters" }, error))
        return false;

    const auto* kindValue = value.find ("kind");
    const auto* idValue = value.find ("id");

    if (idValue == nullptr)
        return fail (error, value, "an fx entry needs an id");

    if (kindValue != nullptr && ! readString (*kindValue, fx.kind, error, "fx kind"))
        return false;

    if (fx.kind != "builtin" && fx.kind != "plugin")
        return fail (error, kindValue != nullptr ? *kindValue : value,
                     "fx kind must be 'builtin' or 'plugin'");

    if (! readString (*idValue, fx.id, error, "fx id"))
        return false;

    if (const auto* name = value.find ("name"))
        if (! readString (*name, fx.name, error, "fx name"))
            return false;

    if (const auto* state = value.find ("pluginState"))
        if (! readString (*state, fx.pluginState, error, "fx pluginState", true))
            return false;

    if (const auto* bypassed = value.find ("bypassed"))
        if (! readBool (*bypassed, fx.bypassed, error, "fx bypassed"))
            return false;

    if (const auto* parameters = value.find ("parameters"))
    {
        if (parameters->kind != YamlKind::mapping)
            return fail (error, *parameters, "fx parameters must be a mapping");

        for (const auto& [key, parameterValue] : parameters->mapping)
        {
            if (key.empty())
                return fail (error, parameterValue, "an fx parameter name must not be empty");

            float parsed = 0.0f;

            if (! readFloat (parameterValue, parsed, error, "fx parameter '" + key + "'"))
                return false;

            fx.parameters[key] = parsed;
        }
    }

    return true;
}

} // namespace

bool decodeDocument (const YamlValue& root, SetMapping& out, MappingError& error)
{
    if (root.kind != YamlKind::mapping)
        return fail (error, root, "the document root must be a mapping");

    if (! checkKeys (root, { "schemaVersion", "name", "families", "macros",
                             "pads", "encoders", "banks", "scenes" }, error))
        return false;

    const auto* version = root.find ("schemaVersion");

    if (version == nullptr)
        return fail (error, root, "schemaVersion is required");

    int schemaVersion = 0;

    if (! readInt (*version, schemaVersion, error, "schemaVersion"))
        return false;

    if (schemaVersion != SetMapping::supportedSchemaVersion)
        return fail (error, *version,
                     "unsupported schemaVersion " + std::to_string (schemaVersion)
                         + " (expected " + std::to_string (SetMapping::supportedSchemaVersion) + ")");

    out.schemaVersion = schemaVersion;

    if (const auto* name = root.find ("name"))
        if (! readString (*name, out.name, error, "name"))
            return false;

    // --- families --------------------------------------------------------
    const auto* families = root.find ("families");

    if (families == nullptr)
        return fail (error, root, "families is required");

    if (families->kind != YamlKind::mapping)
        return fail (error, *families, "families must be a mapping of family ids");

    if (families->mapping.empty())
        return fail (error, *families, "families must not be empty");

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
                || ! readString (*name, instrument.name, error, "instrument name")
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

            // --- Patch persistence (T119) --------------------------------
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

    // --- macros ----------------------------------------------------------
    const auto* macros = root.find ("macros");

    if (macros == nullptr)
        return fail (error, root, "macros is required");

    if (macros->kind != YamlKind::mapping)
        return fail (error, *macros, "macros must be a mapping of macro ids");

    for (const auto& [macroId, macroValue] : macros->mapping)
    {
        if (macroId.empty())
            return fail (error, macroValue, "a macro id must not be empty");

        if (macroValue.kind != YamlKind::mapping)
            return fail (error, macroValue, "macro '" + macroId + "' must be a mapping");

        if (! checkKeys (macroValue, { "name", "parameter", "min", "max", "default" }, error))
            return false;

        MacroSpec macro;
        macro.id = macroId;

        const auto* name = macroValue.find ("name");
        const auto* parameter = macroValue.find ("parameter");

        if (name == nullptr || parameter == nullptr)
            return fail (error, macroValue,
                         "macro '" + macroId + "' needs name and parameter");

        if (! readString (*name, macro.name, error, "macro name")
            || ! readString (*parameter, macro.parameter, error, "macro parameter"))
            return false;

        if (const auto* minimum = macroValue.find ("min"))
            if (! readFloat (*minimum, macro.minimum, error, "macro min"))
                return false;

        if (const auto* maximum = macroValue.find ("max"))
            if (! readFloat (*maximum, macro.maximum, error, "macro max"))
                return false;

        if (! (macro.minimum < macro.maximum))
            return fail (error, macroValue,
                         "macro '" + macroId + "' needs min < max");

        macro.defaultValue = macro.minimum;

        if (const auto* defaultValue = macroValue.find ("default"))
        {
            if (! readFloat (*defaultValue, macro.defaultValue, error, "macro default"))
                return false;

            if (macro.defaultValue < macro.minimum || macro.defaultValue > macro.maximum)
                return fail (error, *defaultValue,
                             "macro '" + macroId + "' default is outside its range");
        }

        out.macros.emplace (macro.id, std::move (macro));
    }

    // --- pads ------------------------------------------------------------
    const auto* pads = root.find ("pads");

    if (pads == nullptr)
        return fail (error, root, "pads is required");

    if (pads->kind != YamlKind::sequence)
        return fail (error, *pads, "pads must be a sequence");

    for (const auto& padValue : pads->sequence)
    {
        if (padValue.kind != YamlKind::mapping)
            return fail (error, padValue, "a pad entry must be a mapping");

        if (! checkKeys (padValue, { "pad", "family", "instrument" }, error))
            return false;

        const auto* padNumber = padValue.find ("pad");
        const auto* family = padValue.find ("family");
        const auto* instrument = padValue.find ("instrument");

        if (padNumber == nullptr || family == nullptr || instrument == nullptr)
            return fail (error, padValue, "a pad entry needs pad, family and instrument");

        PadBinding binding;

        if (! readInt (*padNumber, binding.pad, error, "pad")
            || ! readString (*family, binding.family, error, "pad family")
            || ! readString (*instrument, binding.instrument, error, "pad instrument"))
            return false;

        if (binding.pad < 0 || binding.pad >= SetMapping::maximumPads)
            return fail (error, *padNumber,
                         "pad must be within 0.." + std::to_string (SetMapping::maximumPads - 1));

        if (out.padBinding (binding.pad) != nullptr)
            return fail (error, *padNumber,
                         "duplicate pad binding for pad " + std::to_string (binding.pad));

        const auto familyEntry = out.families.find (binding.family);

        if (familyEntry == out.families.end())
            return fail (error, *family,
                         "pad " + std::to_string (binding.pad) + " names unknown family '"
                             + binding.family + "'");

        const auto* instrumentSpec = familyEntry->second.findInstrument (binding.instrument);

        if (instrumentSpec == nullptr)
            return fail (error, *instrument,
                         "pad " + std::to_string (binding.pad) + " names unknown instrument '"
                             + binding.family + "/" + binding.instrument + "'");

        binding.slot = instrumentSpec->slot;

        out.pads[static_cast<std::size_t> (binding.pad)] = std::move (binding);
    }

    // --- encoders --------------------------------------------------------
    const auto* encoders = root.find ("encoders");

    if (encoders == nullptr)
        return fail (error, root, "encoders is required");

    if (encoders->kind != YamlKind::sequence)
        return fail (error, *encoders, "encoders must be a sequence");

    for (const auto& encoderValue : encoders->sequence)
    {
        if (encoderValue.kind != YamlKind::mapping)
            return fail (error, encoderValue, "an encoder entry must be a mapping");

        if (! checkKeys (encoderValue, { "encoder", "macro" }, error))
            return false;

        const auto* encoderNumber = encoderValue.find ("encoder");
        const auto* macro = encoderValue.find ("macro");

        if (encoderNumber == nullptr || macro == nullptr)
            return fail (error, encoderValue, "an encoder entry needs encoder and macro");

        EncoderBinding binding;

        if (! readInt (*encoderNumber, binding.encoder, error, "encoder")
            || ! readString (*macro, binding.macro, error, "encoder macro"))
            return false;

        if (binding.encoder < 0 || binding.encoder >= SetMapping::maximumEncoders)
            return fail (error, *encoderNumber,
                         "encoder must be within 0.."
                             + std::to_string (SetMapping::maximumEncoders - 1));

        if (out.encoderBinding (binding.encoder) != nullptr)
            return fail (error, *encoderNumber,
                         "duplicate encoder binding for encoder "
                             + std::to_string (binding.encoder));

        if (out.macros.find (binding.macro) == out.macros.end())
            return fail (error, *macro,
                         "encoder " + std::to_string (binding.encoder)
                             + " names unknown macro '" + binding.macro + "'");

        out.encoders[static_cast<std::size_t> (binding.encoder)] = std::move (binding);
    }

    // --- macro banks (top row) -------------------------------------------
    if (const auto* banks = root.find ("banks"))
    {
        if (banks->kind != YamlKind::sequence)
            return fail (error, *banks, "banks must be a sequence");

        for (const auto& bankValue : banks->sequence)
        {
            if (bankValue.kind != YamlKind::mapping)
                return fail (error, bankValue, "a bank entry must be a mapping");

            if (! checkKeys (bankValue, { "bank", "name", "macros" }, error))
                return false;

            const auto* bankNumber = bankValue.find ("bank");
            const auto* name = bankValue.find ("name");
            const auto* macroList = bankValue.find ("macros");

            if (bankNumber == nullptr || name == nullptr || macroList == nullptr)
                return fail (error, bankValue, "a bank entry needs bank, name and macros");

            MacroBankSpec bank;

            if (! readInt (*bankNumber, bank.bank, error, "bank")
                || ! readString (*name, bank.name, error, "bank name"))
                return false;

            if (bank.bank < 0 || bank.bank >= SetMapping::maximumBanks)
                return fail (error, *bankNumber,
                             "bank must be within 0.."
                                 + std::to_string (SetMapping::maximumBanks - 1));

            if (out.bankBinding (bank.bank) != nullptr)
                return fail (error, *bankNumber,
                             "duplicate bank binding for bank "
                                 + std::to_string (bank.bank));

            if (macroList->kind != YamlKind::sequence || macroList->sequence.empty())
                return fail (error, *macroList,
                             "bank " + std::to_string (bank.bank)
                                 + " needs a non-empty macros list");

            for (const auto& macroValue : macroList->sequence)
            {
                std::string macroId;

                if (! readString (macroValue, macroId, error, "bank macro"))
                    return false;

                if (out.macros.find (macroId) == out.macros.end())
                    return fail (error, macroValue,
                                 "bank " + std::to_string (bank.bank)
                                     + " names unknown macro '" + macroId + "'");

                bank.macros.push_back (std::move (macroId));
            }

            out.banks[static_cast<std::size_t> (bank.bank)] = std::move (bank);
        }
    }

    // --- scenes (right column) --------------------------------------------
    if (const auto* scenes = root.find ("scenes"))
    {
        if (scenes->kind != YamlKind::sequence)
            return fail (error, *scenes, "scenes must be a sequence");

        for (const auto& sceneValue : scenes->sequence)
        {
            if (sceneValue.kind != YamlKind::mapping)
                return fail (error, sceneValue, "a scene entry must be a mapping");

            if (! checkKeys (sceneValue, { "scene", "name", "family" }, error))
                return false;

            const auto* sceneNumber = sceneValue.find ("scene");
            const auto* name = sceneValue.find ("name");
            const auto* family = sceneValue.find ("family");

            if (sceneNumber == nullptr || name == nullptr || family == nullptr)
                return fail (error, sceneValue, "a scene entry needs scene, name and family");

            SceneSpec scene;

            if (! readInt (*sceneNumber, scene.scene, error, "scene")
                || ! readString (*name, scene.name, error, "scene name")
                || ! readString (*family, scene.family, error, "scene family"))
                return false;

            if (scene.scene < 0 || scene.scene >= SetMapping::maximumScenes)
                return fail (error, *sceneNumber,
                             "scene must be within 0.."
                                 + std::to_string (SetMapping::maximumScenes - 1));

            if (out.sceneBinding (scene.scene) != nullptr)
                return fail (error, *sceneNumber,
                             "duplicate scene binding for scene "
                                 + std::to_string (scene.scene));

            if (out.families.find (scene.family) == out.families.end())
                return fail (error, *family,
                             "scene " + std::to_string (scene.scene)
                                 + " names unknown family '" + scene.family + "'");

            out.scenes[static_cast<std::size_t> (scene.scene)] = std::move (scene);
        }
    }

    return true;
}

} // namespace Controller::MappingDetail
