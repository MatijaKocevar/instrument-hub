#pragma once

#include "controller/MappingYaml.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Controller::MappingDetail {

enum class YamlKind { nullValue, scalar, mapping, sequence };

struct YamlValue {
    YamlKind kind = YamlKind::nullValue;
    std::string scalar;
    std::vector<std::pair<std::string, YamlValue>> mapping;
    std::vector<YamlValue> sequence;
    int line = 0;

    const YamlValue* find(std::string_view key) const noexcept {
        for (const auto& [entryKey, value] : mapping)
            if (entryKey == key) return &value;

        return nullptr;
    }
};

struct SourceLine {
    int indent = 0;
    std::string text;
    int number = 0;
};

}  // namespace Controller::MappingDetail
