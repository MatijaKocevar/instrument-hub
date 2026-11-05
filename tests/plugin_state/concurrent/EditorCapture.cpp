#include "EditorCapture.h"

#include "ConcurrentFixture.h"
#include "plugins/SandboxHost.h"

#include <vector>

namespace ConcurrentInstances
{

void runEditorCapture (ihub::Session& session, const Check& check)
{
    std::vector<int> processIds (static_cast<std::size_t> (kSlots), 0);

    for (int slot = 0; slot < kSlots; ++slot)
    {
        applyPreset (session, slot);
        processIds[static_cast<std::size_t> (slot)] =
            session.getSandboxProcessId (slot);
    }

    for (int slot = 0; slot < kSlots; ++slot)
    {
        auto* host = session.getSandboxSlotControl (slot);

        if (host == nullptr)
        {
            check (false, "slot " + std::to_string (slot) + " is hosted");
            continue;
        }

        std::vector<std::uint64_t> revisions (static_cast<std::size_t> (kSlots), 0);

        for (int other = 0; other < kSlots; ++other)
            if (auto* candidate = session.getSandboxSlotControl (other);
                candidate != nullptr)
                revisions[static_cast<std::size_t> (other)] =
                    candidate->getLastCommittedStateRevision();

        std::string error;
        check (session.openEditor (slot, error),
               "slot " + std::to_string (slot) + " opens its native editor"
                   + (error.empty() ? std::string() : " (" + error + ")"));

        check (session.isEditorOpen (slot),
               "slot " + std::to_string (slot) + " reports its editor open");

        bool othersUnchanged = true;

        for (int other = 0; other < kSlots; ++other)
            if (other != slot
                && session.getSandboxProcessId (other)
                       != processIds[static_cast<std::size_t> (other)])
                othersUnchanged = false;

        check (othersUnchanged,
               "opening slot " + std::to_string (slot)
                   + "'s editor does not restart another slot");

        applyPreset (session, slot);

        check (session.closeEditor (slot, error),
               "slot " + std::to_string (slot) + " closes its editor"
                   + (error.empty() ? std::string() : " (" + error + ")"));

        check (! session.isEditorOpen (slot),
               "slot " + std::to_string (slot)
                   + " reports its editor closed immediately");

        const auto revision = host->getLastCommittedStateRevision();

        check (revision > revisions[static_cast<std::size_t> (slot)],
               "closing slot " + std::to_string (slot)
                   + " commits a new plugin state revision");

        check (host->getLastCommittedState().getSize() > 0,
               "slot " + std::to_string (slot) + " has committed state bytes");

        check (session.hasPluginState (slot),
               "slot " + std::to_string (slot) + " state reached the store");

        bool revisionsIsolated = true;

        for (int other = 0; other < kSlots; ++other)
        {
            if (other == slot)
                continue;

            if (auto* candidate = session.getSandboxSlotControl (other);
                candidate != nullptr
                && candidate->getLastCommittedStateRevision()
                       != revisions[static_cast<std::size_t> (other)])
                revisionsIsolated = false;
        }

        check (revisionsIsolated,
               "closing slot " + std::to_string (slot)
                   + " does not touch another slot's committed state");

        bool processesIsolated = true;

        for (int other = 0; other < kSlots; ++other)
            if (session.getSandboxProcessId (other)
                != processIds[static_cast<std::size_t> (other)])
                processesIsolated = false;

        check (processesIsolated,
               "opening and closing slot " + std::to_string (slot)
                   + " keeps every sandbox process");
    }

    std::vector<juce::String> hashes;

    for (int slot = 0; slot < kSlots; ++slot)
        hashes.push_back (storedStateHash (session, slot));

    bool distinct = true;

    for (std::size_t left = 0; left < hashes.size(); ++left)
        for (std::size_t right = left + 1; right < hashes.size(); ++right)
            if (hashes[left].isEmpty() || hashes[left] == hashes[right])
                distinct = false;

    check (distinct, "every slot keeps a distinct committed state hash");
}

} // namespace ConcurrentInstances
