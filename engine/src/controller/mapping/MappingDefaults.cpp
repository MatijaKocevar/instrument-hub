#include "controller/Mapping.h"

#include <cstdlib>
#include <system_error>

namespace Controller
{

std::filesystem::path Mapping::findDefaultSetFile()
{
    if (const char* environment = std::getenv ("INSTRUMENT_HUB_SET"))
    {
        const std::filesystem::path fromEnvironment (environment);

        if (! fromEnvironment.empty())
            return fromEnvironment;
    }

    std::error_code ec;
    const auto directory = std::filesystem::current_path (ec);

    if (ec)
        return {};

    const std::filesystem::path candidates[] {
        directory / "set.yaml",
        directory / "shared" / "set.yaml"
    };

    for (const auto& candidate : candidates)
        if (std::filesystem::exists (candidate, ec))
            return candidate;

    return {};
}

} // namespace Controller
