#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>

namespace Controller::MappingDetail
{

bool decodeDocument (const YamlValue& root, SetMapping& out, MappingError& error)
{
    if (root.kind != YamlKind::mapping)
        return fail (error, root, "the document root must be a mapping");

    if (! checkKeys (root, { "schemaVersion", "name", "layout", "families", "macros",
                             "tracks", "pads", "encoders", "banks", "scenes" }, error))
        return false;

    const auto* version = root.find ("schemaVersion");

    if (version == nullptr)
        return fail (error, root, "schemaVersion is required");

    int schemaVersion = 0;

    if (! readInt (*version, schemaVersion, error, "schemaVersion"))
        return false;

    if (schemaVersion != SetMapping::supportedSchemaVersion
        && schemaVersion != SetMapping::supportedTrackSchemaVersion)
        return fail (error, *version,
                     "unsupported schemaVersion " + std::to_string (schemaVersion)
                         + " (expected " + std::to_string (SetMapping::supportedSchemaVersion)
                         + " or " + std::to_string (SetMapping::supportedTrackSchemaVersion)
                         + ")");

    out.schemaVersion = schemaVersion;

    if (const auto* layout = root.find ("layout"))
    {
        if (schemaVersion != SetMapping::supportedTrackSchemaVersion)
            return fail (error, *layout, "layout is only valid for schemaVersion 2");

        std::string layoutName;

        if (! readString (*layout, layoutName, error, "layout"))
            return false;

        if (layoutName != "tracks")
            return fail (error, *layout, "layout must be tracks");

        out.layout = MappingLayout::tracks;
    }
    else if (schemaVersion == SetMapping::supportedTrackSchemaVersion)
    {
        return fail (error, root, "schemaVersion 2 needs layout: tracks");
    }

    if (const auto* name = root.find ("name"))
        if (! readString (*name, out.name, error, "name"))
            return false;

    return decodeTracks (root, out, error)
           && decodeFamilies (root, out, error)
           && decodeMacros (root, out, error)
           && decodePads (root, out, error)
           && decodeEncoders (root, out, error)
           && decodeBanks (root, out, error)
           && decodeScenes (root, out, error);
}

}
