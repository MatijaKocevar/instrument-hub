#include "settings/AtomicFileWrite.h"

#if !JUCE_WINDOWS
#include <sys/stat.h>
#endif

namespace ihub::settings {
namespace {

bool writeToFile(const juce::File& file, const juce::String& contents) {
    juce::FileOutputStream stream(file);

    if (!stream.openedOk()) return false;

    if (!stream.write(contents.toRawUTF8(), static_cast<size_t>(contents.getNumBytesAsUTF8())))
        return false;

    stream.flush();
    return !stream.getStatus().failed();
}

bool restrictToOwner(const juce::File& file) {
#if JUCE_WINDOWS
    juce::ignoreUnused(file);
    return true;
#else
    return ::chmod(file.getFullPathName().toRawUTF8(), S_IRUSR | S_IWUSR) == 0;
#endif
}

}  // namespace

bool writeFileAtomically(const juce::File& target, const juce::String& contents,
                         const AtomicWriter& writer) {
    const auto directory = target.getParentDirectory();

    if (!directory.createDirectory()) return false;

    const auto temporary = directory.getChildFile(target.getFileName() + ".tmp");

    if (temporary.exists() && !temporary.deleteRecursively()) return false;

    const auto write = writer ? writer : AtomicWriter(writeToFile);

    if (!write(temporary, contents) || !temporary.existsAsFile()) {
        temporary.deleteFile();
        return false;
    }

    if (!restrictToOwner(temporary)) {
        temporary.deleteFile();
        return false;
    }

    if (!temporary.replaceFileIn(target)) {
        temporary.deleteFile();
        return false;
    }

    return true;
}

}  // namespace ihub::settings
