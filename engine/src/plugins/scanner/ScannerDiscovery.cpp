#include "../Scanner.h"

#include <algorithm>

namespace ihub::plugins {
namespace {
bool isVst3Bundle(const juce::File& file) { return file.hasFileExtension("vst3"); }

bool comparePaths(const juce::File& a, const juce::File& b) {
    return a.getFullPathName() < b.getFullPathName();
}
}  // namespace

juce::StringArray Scanner::discoverFiles(const juce::StringArray& directories) {
    std::vector<juce::File> candidates;

    auto addCandidate = [&candidates](const juce::File& file) {
        if (isVst3Bundle(file) &&
            std::none_of(candidates.begin(), candidates.end(), [&file](const juce::File& other) {
                return other.getFullPathName() == file.getFullPathName();
            })) {
            candidates.push_back(file);
        }
    };

    auto scanOneDirectory = [this, &addCandidate](const juce::File& directory) {
        for (const auto& path : vst3Format->searchPathsForPlugins(
                 juce::FileSearchPath(directory.getFullPathName()), options.recursive, true))
            addCandidate(juce::File(path));
    };

    if (directories.isEmpty()) {
        const auto defaults = vst3Format->getDefaultLocationsToSearch();

        for (int i = 0; i < defaults.getNumPaths(); ++i) {
            const auto directory = juce::File(defaults[i]);

            if (directory.isDirectory()) scanOneDirectory(directory);
        }
    } else {
        for (const auto& path : directories) {
            const juce::File file(path);

            if (vst3Format->fileMightContainThisPluginType(file.getFullPathName())) {
                addCandidate(file);
            } else if (file.isDirectory()) {
                scanOneDirectory(file);
            } else {
                log("scanner: skipping missing path " + file.getFullPathName());
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), comparePaths);

    juce::StringArray result;

    for (const auto& candidate : candidates) result.add(candidate.getFullPathName());

    return result;
}

ScanReport Scanner::scan(const juce::FileSearchPath& directories) {
    juce::StringArray paths;

    for (int i = 0; i < directories.getNumPaths(); ++i) paths.add(directories[i].getFullPathName());

    return scan(paths);
}

}  // namespace ihub::plugins
