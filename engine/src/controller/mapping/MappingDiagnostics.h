#pragma once

#include "controller/mapping/MappingYamlInternal.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace Controller::MappingDetail
{

bool fail (MappingError& error, const YamlValue& value, const std::string& message);

bool checkKeys (const YamlValue& value,
                std::initializer_list<std::string_view> allowed,
                MappingError& error);

bool readString (const YamlValue& value, std::string& out, MappingError& error,
                 const std::string& what, bool allowEmpty = false);

bool readInt (const YamlValue& value, int& out, MappingError& error, const std::string& what);

bool readFloat (const YamlValue& value, float& out, MappingError& error, const std::string& what);

bool readBool (const YamlValue& value, bool& out, MappingError& error, const std::string& what);

bool readColourIndex (const YamlValue& value, int& out, MappingError& error,
                      const std::string& what);

} // namespace Controller::MappingDetail
