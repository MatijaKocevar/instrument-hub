#include "controller/mapping/MappingFile.h"

#include <fstream>
#include <functional>
#include <sstream>

namespace Controller::MappingDetail
{

bool readFileContents (const std::filesystem::path& file, std::string& out, std::string& errorText)
{
    std::ifstream stream (file, std::ios::binary);

    if (! stream)
    {
        errorText = "cannot open '" + file.string() + "'";
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    if (stream.bad())
    {
        errorText = "cannot read '" + file.string() + "'";
        return false;
    }

    out = buffer.str();
    return true;
}

std::uint64_t hashContents (const std::string& contents)
{
    return static_cast<std::uint64_t> (std::hash<std::string> {} (contents));
}


} // namespace Controller::MappingDetail
