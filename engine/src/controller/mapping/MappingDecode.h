#pragma once

#include "controller/mapping/MappingYamlInternal.h"

namespace Controller::MappingDetail
{

bool decodeDocument (const YamlValue& root, SetMapping& out, MappingError& error);

bool decodeTracks (const YamlValue& root, SetMapping& out, MappingError& error);

bool decodeMacros (const YamlValue& root, SetMapping& out, MappingError& error);

bool decodePads (const YamlValue& root, SetMapping& out, MappingError& error);

bool decodeEncoders (const YamlValue& root, SetMapping& out, MappingError& error);

bool decodeBanks (const YamlValue& root, SetMapping& out, MappingError& error);

bool decodeScenes (const YamlValue& root, SetMapping& out, MappingError& error);

}
