#include "TestSupport.h"

namespace SettingsTest
{

void runAtomicReplaceChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = settingsFile (directory);
    const auto temporary = directory.getChildFile ("settings.json.tmp");

    ihub::settings::SettingsStore initial (file);

    check (initial.load() == ihub::settings::LoadStatus::missing,
           "the atomic replace test starts from a missing file");

    initial.settings().mixer.masterGain = 0.4;
    check (initial.save(), "the first settings save succeeds");

    const auto original = readTextFile (file);

    juce::String temporaryPath;
    bool temporaryInTargetDirectory = false;
    bool temporaryDiffersFromTarget = false;

    const ihub::settings::AtomicWriter failingWriter =
        [&temporaryPath, &temporaryInTargetDirectory, &temporaryDiffersFromTarget, &file]
        (const juce::File& path, const juce::String&)
        {
            temporaryPath = path.getFullPathName();
            temporaryInTargetDirectory =
                path.getParentDirectory() == file.getParentDirectory();
            temporaryDiffersFromTarget = path != file;

            const auto partial = juce::String ("{\"version\":1,\"audio\":");
            juce::FileOutputStream stream (path);

            if (stream.openedOk())
                (void) stream.write (partial.toRawUTF8(),
                                     static_cast<size_t> (partial.getNumBytesAsUTF8()));

            return false;
        };

    ihub::settings::SettingsStore store (file, failingWriter);

    check (store.load() == ihub::settings::LoadStatus::loaded,
           "the existing settings load before the simulated failure");

    store.settings().mixer.masterGain = 0.9;

    check (! store.save(), "a failed atomic write reports failure");
    check (store.getLastError().isNotEmpty(),
           "a failed atomic write reports a reason");
    check (! temporaryPath.isEmpty(),
           "the atomic write goes through a temporary file");
    check (temporaryInTargetDirectory,
           "the temporary file lives next to the target file");
    check (temporaryDiffersFromTarget,
           "the temporary file is not the target file");
    check (! temporary.exists(),
           "a failed write removes the temporary file");
    check (readTextFile (file) == original,
           "a failed write leaves the previous file byte-identical");
    check (! directory.getChildFile ("settings.json.bad").exists(),
           "a failed write does not quarantine the previous file");

    ihub::settings::SettingsStore reloaded (file);

    check (reloaded.load() == ihub::settings::LoadStatus::loaded,
           "the previous file still loads after the failure");
    check (nearlyEqual (reloaded.settings().mixer.masterGain, 0.4),
           "the previous settings survive the failed write");

    directory.deleteRecursively();
}

}
