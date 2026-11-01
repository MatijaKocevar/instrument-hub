#pragma once

#include "MappingModel.h"

#include <filesystem>
#include <string>

namespace Controller
{

struct MappingError
{
    int line = 0;
    std::string message;

    bool isEmpty() const noexcept { return message.empty(); }
};

bool parseSetMappingYaml (const std::string& yaml, SetMapping& result, MappingError& error);

bool loadSetMappingFile (const std::filesystem::path& file,
                         SetMapping& result,
                         MappingError& error);

}
