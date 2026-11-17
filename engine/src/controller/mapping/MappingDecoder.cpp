#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>

namespace Controller::MappingDetail {

bool decodeDocument(const YamlValue& root, SetMapping& out,
                    MappingError& error) {
  if (root.kind != YamlKind::mapping)
    return fail(error, root, "the document root must be a mapping");

  if (!checkKeys(root,
                 {"schemaVersion", "name", "layout", "macros", "tracks", "pads",
                  "encoders", "banks", "scenes"},
                 error))
    return false;

  const auto* version = root.find("schemaVersion");

  if (version == nullptr) return fail(error, root, "schemaVersion is required");

  int schemaVersion = 0;

  if (!readInt(*version, schemaVersion, error, "schemaVersion")) return false;

  if (schemaVersion != SetMapping::supportedSchemaVersion)
    return fail(error, *version,
                "unsupported schemaVersion " + std::to_string(schemaVersion) +
                    " (expected " +
                    std::to_string(SetMapping::supportedSchemaVersion) + ")");

  out.schemaVersion = schemaVersion;

  const auto* layout = root.find("layout");

  if (layout == nullptr) return fail(error, root, "layout: tracks is required");

  std::string layoutName;

  if (!readString(*layout, layoutName, error, "layout")) return false;

  if (layoutName != "tracks")
    return fail(error, *layout, "layout must be tracks");

  out.layout = MappingLayout::tracks;

  if (const auto* name = root.find("name"))
    if (!readString(*name, out.name, error, "name")) return false;

  return decodeTracks(root, out, error) && decodeMacros(root, out, error) &&
         decodePads(root, out, error) && decodeEncoders(root, out, error) &&
         decodeBanks(root, out, error) && decodeScenes(root, out, error);
}

}  // namespace Controller::MappingDetail
