#include "controller/Mapping.h"

#include "controller/mapping/MappingFile.h"

#include <system_error>
#include <utility>

namespace Controller {

namespace {
std::filesystem::file_time_type modificationTime(
    const std::filesystem::path& file, std::error_code& error) {
  error.clear();
  return std::filesystem::last_write_time(file, error);
}
}  // namespace

bool Mapping::load(const std::filesystem::path& file, MappingError* error) {
  return openSource(file, {}, error);
}

bool Mapping::loadProject(const std::filesystem::path& projectFile,
                          MappingError* error) {
  return openSource(projectFile, "mapping.yaml", error);
}

bool Mapping::openSource(const std::filesystem::path& file,
                         std::string newArchiveEntry, MappingError* error) {
  std::error_code ec;
  auto absolute = std::filesystem::absolute(file, ec);

  if (ec) absolute = file;

  {
    const std::lock_guard<std::mutex> lock(mutex);
    path = std::move(absolute);
    archiveEntry = std::move(newArchiveEntry);
    lastHash = 0;
    hasAttempted = false;
    hasLoaded = false;
    hasWriteTime = false;
    activePad.store(-1, std::memory_order_release);
    activeBank.store(-1, std::memory_order_release);
    activeScene.store(-1, std::memory_order_release);
  }

  return reloadInternal(false, error);
}

bool Mapping::reload(MappingError* error) {
  const bool hot = [this] {
    const std::lock_guard<std::mutex> lock(mutex);
    return hasLoaded;
  }();

  return reloadInternal(hot, error);
}

bool Mapping::reloadInternal(bool hotReload, MappingError* error) {
  std::filesystem::path currentPath;
  std::string currentEntry;

  {
    const std::lock_guard<std::mutex> lock(mutex);
    currentPath = path;
    currentEntry = archiveEntry;
  }

  if (currentPath.empty()) {
    const MappingError failure{0, "no set file has been loaded"};
    reportError(failure, 0, false);

    if (error != nullptr) *error = failure;

    return false;
  }

  std::string contents;
  std::string readError;

  const bool read =
      currentEntry.empty()
          ? MappingDetail::readFileContents(currentPath, contents, readError)
          : MappingDetail::readArchiveEntry(currentPath, currentEntry, contents,
                                            readError);

  if (!read) {
    const MappingError failure{0, readError};
    reportError(failure, 0, false);

    if (error != nullptr) *error = failure;

    return false;
  }

  std::error_code timeError;
  const auto currentTime = modificationTime(currentPath, timeError);

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (!timeError) {
      writeTime = currentTime;
      hasWriteTime = true;
    }
  }

  return applyText(contents, hotReload, MappingDetail::hashContents(contents),
                   true, error);
}

bool Mapping::poll() {
  std::filesystem::path currentPath;
  std::string currentEntry;
  bool hasCurrentWriteTime = false;
  std::filesystem::file_time_type previousWriteTime{};

  {
    const std::lock_guard<std::mutex> lock(mutex);
    currentPath = path;
    currentEntry = archiveEntry;
    hasCurrentWriteTime = hasWriteTime;
    previousWriteTime = writeTime;
  }

  if (currentPath.empty()) return false;

  if (!currentEntry.empty()) {
    std::error_code timeError;
    const auto currentTime = modificationTime(currentPath, timeError);

    if (!timeError && hasCurrentWriteTime && currentTime == previousWriteTime)
      return false;
  }

  std::string contents;
  std::string readError;

  const bool read =
      currentEntry.empty()
          ? MappingDetail::readFileContents(currentPath, contents, readError)
          : MappingDetail::readArchiveEntry(currentPath, currentEntry, contents,
                                            readError);

  if (!read) {
    const MappingError failure{0, readError};
    reportError(failure, 0, false);
    return false;
  }

  if (!currentEntry.empty()) {
    std::error_code timeError;
    const auto currentTime = modificationTime(currentPath, timeError);

    if (!timeError) {
      const std::lock_guard<std::mutex> lock(mutex);
      writeTime = currentTime;
      hasWriteTime = true;
    }
  }

  const auto hash = MappingDetail::hashContents(contents);
  bool hot = false;

  {
    const std::lock_guard<std::mutex> lock(mutex);

    if (hasAttempted && hash == lastHash) return false;

    hot = hasLoaded;
  }

  return applyText(contents, hot, hash, false, nullptr);
}

}  // namespace Controller
