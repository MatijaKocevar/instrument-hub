#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>
#include <utility>

namespace Controller::MappingDetail
{

bool decodePads (const YamlValue& root, SetMapping& out, MappingError& error)
{
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

    return true;
}

bool decodeEncoders (const YamlValue& root, SetMapping& out, MappingError& error)
{
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

    return true;
}

}
