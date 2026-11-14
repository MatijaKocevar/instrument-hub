#pragma once

#include "controller/mapping/MappingYamlInternal.h"

#include <string>

namespace Controller::MappingDetail
{

    bool parseYaml (const std::string& yaml, YamlValue& out, MappingError& error);

}
