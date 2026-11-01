#pragma once

#include "MappingYaml.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Controller::MappingDetail
{

// The YAML reader, the document validator and the facade share this small
// in-memory tree. It is an implementation detail: only the Mapping translation
// units include this header.

enum class YamlKind
{
    nullValue,
    scalar,
    mapping,
    sequence
};

struct YamlValue
{
    YamlKind kind = YamlKind::nullValue;
    std::string scalar;
    std::vector<std::pair<std::string, YamlValue>> mapping;
    std::vector<YamlValue> sequence;
    int line = 0;

    const YamlValue* find (std::string_view key) const noexcept
    {
        for (const auto& [entryKey, value] : mapping)
            if (entryKey == key)
                return &value;

        return nullptr;
    }
};

struct SourceLine
{
    int indent = 0;
    std::string text;
    int number = 0;
};

/** Validates the parsed tree and fills a `SetMapping`. */
bool decodeDocument (const YamlValue& root, SetMapping& out, MappingError& error);

/** Reads a whole file; on failure `errorText` describes why. */
bool readFileContents (const std::filesystem::path& file, std::string& out, std::string& errorText);

/** Hash used to detect an unchanged file between polls. */
std::uint64_t hashContents (const std::string& contents);

} // namespace Controller::MappingDetail
