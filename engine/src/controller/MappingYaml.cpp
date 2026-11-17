#include "MappingYaml.h"

#include "controller/mapping/MappingDecode.h"
#include "controller/mapping/MappingFile.h"
#include "controller/mapping/YamlParser.h"

#include <utility>

namespace Controller {

bool parseSetMappingYaml(const std::string& yaml, SetMapping& result, MappingError& error) {
    error = {};

    MappingDetail::YamlValue root;

    if (!MappingDetail::parseYaml(yaml, root, error)) return false;

    SetMapping parsed;

    if (!MappingDetail::decodeDocument(root, parsed, error)) return false;

    result = std::move(parsed);
    return true;
}

bool loadSetMappingFile(const std::filesystem::path& file, SetMapping& result,
                        MappingError& error) {
    error = {};

    std::string contents;
    std::string readError;

    if (!MappingDetail::readFileContents(file, contents, readError)) {
        error = {0, readError};
        return false;
    }

    return parseSetMappingYaml(contents, result, error);
}

}  // namespace Controller
