#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>
#include <utility>

namespace Controller::MappingDetail
{

namespace
{

bool resolveTrack (const YamlValue& value, const SetMapping& out, int pad,
                   const TrackSpec*& track, MappingError& error)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value,
                     "pad " + std::to_string (pad) + " track must be a track id or slot");

    if (const auto* byId = out.trackForId (value.scalar))
    {
        track = byId;
        return true;
    }

    int slot = -1;

    if (readInt (value, slot, error, "pad track"))
        if (const auto* bySlot = out.trackAtSlot (slot))
        {
            track = bySlot;
            return true;
        }

    return fail (error, value,
                 "pad " + std::to_string (pad) + " names unknown track '"
                     + value.scalar + "'");
}

bool readRole (const YamlValue& padValue, PadRole& role, MappingError& error)
{
    const auto* roleValue = padValue.find ("role");

    if (roleValue == nullptr)
        return fail (error, padValue, "a tracks pad entry needs role");

    std::string roleName;

    if (! readString (*roleValue, roleName, error, "pad role"))
        return false;

    if (roleName == "track")
        role = PadRole::track;
    else if (roleName == "fx")
        role = PadRole::fx;
    else if (roleName == "volume")
        role = PadRole::volume;
    else
        return fail (error, *roleValue,
                     "unknown pad role '" + roleName + "' (expected track, fx or volume)");

    return true;
}

bool decodeTrackPad (const YamlValue& padValue, SetMapping& out, MappingError& error)
{
    PadRole role;

    if (! readRole (padValue, role, error))
        return false;

    if (role == PadRole::track)
    {
        if (! checkKeys (padValue, { "pad", "role", "track" }, error))
            return false;
    }
    else if (role == PadRole::fx)
    {
        if (! checkKeys (padValue, { "pad", "role", "track", "hot" }, error))
            return false;
    }
    else if (! checkKeys (padValue, { "pad", "role", "track", "step" }, error))
    {
        return false;
    }

    const auto* padNumber = padValue.find ("pad");
    const auto* trackValue = padValue.find ("track");

    if (padNumber == nullptr || trackValue == nullptr)
        return fail (error, padValue, "a tracks pad entry needs pad and track");

    PadBinding binding;
    binding.role = role;

    if (! readInt (*padNumber, binding.pad, error, "pad"))
        return false;

    if (binding.pad < 0 || binding.pad >= SetMapping::maximumPads)
        return fail (error, *padNumber,
                     "pad must be within 0.."
                         + std::to_string (SetMapping::maximumPads - 1));

    if (out.padBinding (binding.pad) != nullptr)
        return fail (error, *padNumber,
                     "duplicate pad binding for pad " + std::to_string (binding.pad));

    const TrackSpec* track = nullptr;

    if (! resolveTrack (*trackValue, out, binding.pad, track, error))
        return false;

    binding.track = track->id;
    binding.slot = track->slot;

    const int row = binding.pad / 8;

    if (role == PadRole::track && row != 7)
        return fail (error, *padNumber, "track pads belong on row 7");

    if (role == PadRole::fx && row != 5 && row != 6)
        return fail (error, *padNumber, "fx pads belong on rows 5 and 6");

    if (role == PadRole::volume && row > 4)
        return fail (error, *padNumber, "volume pads belong on rows 0..4");

    if (role == PadRole::fx)
    {
        const auto* hot = padValue.find ("hot");

        if (hot == nullptr)
            return fail (error, padValue, "an fx pad entry needs hot");

        if (! readInt (*hot, binding.hot, error, "fx hot"))
            return false;

        if (binding.hot < 0 || binding.hot > 1)
            return fail (error, *hot, "fx hot must be 0 or 1");
    }
    else if (role == PadRole::volume)
    {
        const auto* step = padValue.find ("step");

        if (step == nullptr)
            return fail (error, padValue, "a volume pad entry needs step");

        if (! readInt (*step, binding.step, error, "volume step"))
            return false;

        if (binding.step < 0 || binding.step > SetMapping::maximumVolumeStep)
            return fail (error, *step,
                         "volume step must be within 0.."
                             + std::to_string (SetMapping::maximumVolumeStep));
    }

    out.pads[static_cast<std::size_t> (binding.pad)] = std::move (binding);
    return true;
}

}

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

        if (! decodeTrackPad (padValue, out, error))
            return false;
    }

    return true;
}

bool decodeEncoders (const YamlValue& root, SetMapping& out, MappingError& error)
{
    const auto* encoders = root.find ("encoders");

    if (encoders == nullptr || encoders->kind == YamlKind::nullValue)
        return true;

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
