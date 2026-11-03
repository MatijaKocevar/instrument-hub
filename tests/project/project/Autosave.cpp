#include "Autosave.h"

#include "Support.h"

#include "app/project/ProjectAutosave.h"
#include "project/Serializer.h"
#include "project/SetFile.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <string>
#include <vector>

namespace ProjectTest
{

namespace
{

ihub::project::SetFile makeSet (float gain)
{
    ihub::project::SetFile set;
    set.name = "Autosave";

    ihub::project::SlotConfig slot;
    slot.index = 0;
    slot.name = "Patch";
    slot.gain = gain;
    slot.instrument.name = "Patch";
    slot.instrument.kind = ihub::project::SourceKind::initTone;
    slot.instrument.gain = gain;
    set.slots.push_back (std::move (slot));
    return set;
}

void pumpMessages (int milliseconds)
{
    juce::Thread::sleep (milliseconds);
    juce::Timer::callPendingTimersSynchronously();
}

juce::MemoryBlock readFile (const juce::File& file)
{
    juce::MemoryBlock data;
    (void) file.loadFileAsData (data);
    return data;
}

}

void runAutosaveChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto projectFile = directory.getChildFile ("Autosave.ihub");

    juce::String error;

    check (ihub::project::Serializer::save (makeSet (1.0f), projectFile, error),
           "the autosave project starts from a saved set: " + error.toStdString());

    ihub::app::ProjectAutosave autosave (projectFile, 500);
    int captures = 0;
    float nextGain = 0.5f;

    autosave.setCaptureHandler (
        [&captures, &nextGain] (ihub::project::SetFile& set,
                                const std::vector<int>& touchedSlots,
                                juce::String&)
        {
            juce::ignoreUnused (touchedSlots);
            ++captures;
            set = makeSet (nextGain);
            return true;
        });

    check (! autosave.getState().dirty,
           "a freshly loaded project is not dirty");
    check (autosave.getState().saved,
           "a freshly loaded project reports saved");

    for (int move = 0; move < 10; ++move)
        autosave.noteEdit ("patch", 0);

    pumpMessages (900);

    check (autosave.getSaveCount() == 1,
           "ten fader moves produce one file write (got "
               + std::to_string (autosave.getSaveCount()) + ")");
    check (captures == 1,
           "the debounce captures the project once (got "
               + std::to_string (captures) + ")");
    check (! autosave.getState().dirty && autosave.getState().saved,
           "the autosave clears dirty after the write");

    ihub::project::SetFile loaded;

    check (ihub::project::Serializer::load (projectFile, loaded, error),
           "the autosaved project reloads: " + error.toStdString());
    check (loaded.slots.size() == 1
               && std::abs (loaded.slots.front().gain - 0.5f) < 1.0e-5f,
           "the edited fader value reached the project file");

    const auto firstWrite = readFile (projectFile);

    nextGain = 0.25f;
    autosave.noteEdit ("patch", 0);
    pumpMessages (900);

    check (autosave.getSaveCount() == 2, "a second edit writes once more");

    const auto backup = directory.getChildFile ("Autosave.ihub.bak");
    check (backup.existsAsFile(), "the previous file is kept as .ihub.bak");
    check (readFile (backup) == firstWrite,
           "the .bak is byte-identical to the previous project");

    check (ihub::project::Serializer::load (projectFile, loaded, error)
               && std::abs (loaded.slots.front().gain - 0.25f) < 1.0e-5f,
           "the second write replaces the project atomically");

    const auto beforeFailure = readFile (projectFile);
    const auto backupBeforeFailure = readFile (backup);
    const auto savedBeforeFailure = autosave.getSaveCount();

    const auto temporary = directory.getChildFile ("Autosave.ihub.tmp");
    check (temporary.createDirectory(), "the write-failure trap is installed");
    check (temporary.getChildFile ("keep.txt").replaceWithText ("blocked"),
           "the write-failure trap is not empty");

    nextGain = 0.75f;
    autosave.noteEdit ("patch", 0);

    juce::String failureError;
    check (! autosave.saveNow (&failureError),
           "a failed write is reported");
    check (failureError.isNotEmpty(),
           "the failed write returns an error message");
    check (! autosave.getState().saved && autosave.getState().dirty,
           "the failed write leaves the project dirty and not saved");
    check (readFile (projectFile) == beforeFailure,
           "a failed write leaves the previous .ihub byte-identical");
    check (readFile (backup) == backupBeforeFailure,
           "a failed write leaves the previous .bak untouched");

    pumpMessages (700);
    check (autosave.getSaveCount() == savedBeforeFailure,
           "a failed write is not retried silently");

    temporary.deleteRecursively();
    const auto partial = directory.getChildFile ("Autosave.ihub.tmp");
    check (partial.replaceWithText ("not a zip"), "a crash leaves a partial .tmp");
    check (ihub::project::Serializer::load (projectFile, loaded, error),
           "a crash mid-write still leaves a complete .ihub");
    check (readFile (projectFile) == beforeFailure,
           "a crash mid-write leaves the previous .ihub byte-identical");
    partial.deleteFile();

    check (projectFile.setReadOnly (true), "the project can be marked read-only");
    nextGain = 0.9f;
    autosave.noteEdit ("patch", 0);

    juce::String readOnlyError;
    check (! autosave.saveNow (&readOnlyError),
           "a read-only project refuses to save");
    check (readOnlyError.contains ("read-only"),
           "the read-only error explains why the write stopped");
    check (! autosave.getState().saved && autosave.getState().dirty,
           "a read-only project keeps running and reports not saved");
    check (projectFile.setReadOnly (false), "the project can be made writable again");
    autosave.noteEdit ("patch", 0);
    check (autosave.saveNow (nullptr),
           "the project saves again once the read-only mark is removed");

    directory.deleteRecursively();
}

}
