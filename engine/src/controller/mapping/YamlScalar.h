#pragma once

#include "controller/mapping/MappingYamlInternal.h"

#include <string>

namespace Controller::MappingDetail
{

bool decodeScalar (const std::string& raw, YamlValue& out, MappingError& error, int line);

}
