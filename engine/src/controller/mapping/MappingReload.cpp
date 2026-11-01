#include "controller/Mapping.h"

#include "controller/mapping/MappingFile.h"

#include <system_error>
#include <utility>

namespace Controller
{

bool Mapping::load (const std::filesystem::path& file, MappingError* error)
{
    std::error_code ec;
    auto absolute = std::filesystem::absolute (file, ec);

    if (ec)
        absolute = file;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        path = std::move (absolute);
        lastHash = 0;
        hasAttempted = false;
        hasLoaded = false;
        activePad.store (-1, std::memory_order_release);
        activeBank.store (-1, std::memory_order_release);
        activeScene.store (-1, std::memory_order_release);
    }

    return reloadInternal (false, error);
}

bool Mapping::reload (MappingError* error)
{
    const bool hot = [this]
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return hasLoaded;
    }();

    return reloadInternal (hot, error);
}

bool Mapping::reloadInternal (bool hotReload, MappingError* error)
{
    std::filesystem::path currentPath;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentPath = path;
    }

    if (currentPath.empty())
    {
        const MappingError failure { 0, "no set file has been loaded" };
        reportError (failure, 0, false);

        if (error != nullptr)
            *error = failure;

        return false;
    }

    std::string contents;
    std::string readError;

    if (! MappingDetail::readFileContents (currentPath, contents, readError))
    {
        const MappingError failure { 0, readError };
        reportError (failure, 0, false);

        if (error != nullptr)
            *error = failure;

        return false;
    }

    return applyText (contents, hotReload, MappingDetail::hashContents (contents), true, error);
}

bool Mapping::poll()
{
    std::filesystem::path currentPath;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentPath = path;
    }

    if (currentPath.empty())
        return false;

    std::string contents;
    std::string readError;

    if (! MappingDetail::readFileContents (currentPath, contents, readError))
    {
        const MappingError failure { 0, readError };
        reportError (failure, 0, false);
        return false;
    }

    const auto hash = MappingDetail::hashContents (contents);
    bool hot = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (hasAttempted && hash == lastHash)
            return false;

        hot = hasLoaded;
    }

    return applyText (contents, hot, hash, false, nullptr);
}

}
