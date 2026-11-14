#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Controller::MappingDetail
{

    bool
    readFileContents (const std::filesystem::path& file, std::string& out, std::string& errorText);

    bool readArchiveEntry (const std::filesystem::path& archive,
                           const std::string& entryName,
                           std::string& out,
                           std::string& errorText);

    std::uint64_t hashContents (const std::string& contents);

}
