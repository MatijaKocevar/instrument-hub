#pragma once

#include "controller/mapping/MappingYamlInternal.h"

#include <string>
#include <string_view>
#include <vector>

namespace Controller::MappingDetail
{

std::string trim (std::string_view text);

bool isSequenceEntry (const std::string& text);

bool splitKeyValue (const std::string& text, std::string& key, std::string& value);

bool tokenize (const std::string& yaml, std::vector<SourceLine>& lines, MappingError& error);

}
