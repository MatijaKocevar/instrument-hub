#include "Reload.h"

#include "plugins/SandboxHost.h"

#include <cmath>
#include <string>
#include <vector>

namespace ConcurrentInstances
{
namespace
{
bool approximately (double value, double expected, double tolerance)
{
    return std::abs (value - expected) <= tolerance;
}

void checkFrequencies (ihub::Session& session, const Check& check)
{
    for (int slot = 0; slot < kSlots; ++slot)
    {
        const auto pitch = measurePitch (session, slot);
        const auto expected = kFrequencies[static_cast<std::size_t> (slot)];

        check (approximately (pitch, expected, 20.0),
               "slot " + std::to_string (slot) + " renders "
                   + std::to_string (expected) + " Hz (got "
                   + std::to_string (pitch) + " Hz)");
    }
}
}

std::vector<juce::String> runReload (ihub::Session& session,
                                     const Check& check)
{
    std::vector<int> processIds (static_cast<std::size_t> (kSlots), 0);
    std::vector<juce::String> hashes;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        processIds[static_cast<std::size_t> (slot)] =
            session.getSandboxProcessId (slot);
        hashes.push_back (storedStateHash (session, slot));
    }

    check (session.capturePluginState (kReloadSlot),
           "the slot to reload commits its current state");
    hashes[static_cast<std::size_t> (kReloadSlot)] =
        storedStateHash (session, kReloadSlot);

    const auto expected = hashes[static_cast<std::size_t> (kReloadSlot)];
    std::string error;

    check (session.reloadPlugin (kReloadSlot, error),
           "reload restarts only the selected slot"
               + (error.empty() ? std::string() : " (" + error + ")"));

    const auto restartedProcessId = session.getSandboxProcessId (kReloadSlot);

    check (restartedProcessId > 0
               && restartedProcessId
                      != processIds[static_cast<std::size_t> (kReloadSlot)],
           "the reloaded slot has a new sandbox process");

    bool othersKeptProcess = true;

    for (int slot = 0; slot < kSlots; ++slot)
        if (slot != kReloadSlot
            && session.getSandboxProcessId (slot)
                   != processIds[static_cast<std::size_t> (slot)])
            othersKeptProcess = false;

    check (othersKeptProcess,
           "reloading one slot leaves the other seven sandbox processes alone");

    check (storedStateHash (session, kReloadSlot) == expected,
           "the reloaded slot restores its last committed state");

    bool othersKeptState = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (slot == kReloadSlot)
            continue;

        if (storedStateHash (session, slot)
            != hashes[static_cast<std::size_t> (slot)])
            othersKeptState = false;
    }

    check (othersKeptState,
           "reloading one slot leaves the other seven stored states untouched");

    if (auto* host = session.getSandboxSlotControl (kReloadSlot); host != nullptr)
    {
        check (stateHash (host->getLastCommittedState()) == expected,
               "the new sandbox starts from the committed blob");
        check (! host->isFailed(), "the reloaded sandbox is healthy");
    }
    else
    {
        check (false, "the reloaded slot has a sandbox host");
    }

    check (session.getSlotError (kReloadSlot).isEmpty(),
           "the reloaded slot has no error");

    checkFrequencies (session, check);

    check (session.savePluginStates(), "the eight states are saved to the sidecar");

    return hashes;
}

void runRestored (ihub::Session& session,
                  const std::vector<juce::String>& expectedHashes,
                  const Check& check)
{
    bool restored = expectedHashes.size() == static_cast<std::size_t> (kSlots);

    for (int slot = 0; slot < kSlots; ++slot)
        restored = restored && session.hasPluginState (slot)
                   && storedStateHash (session, slot)
                          == expectedHashes[static_cast<std::size_t> (slot)];

    check (restored, "restarting the session restores all eight state blobs");

    bool distinct = true;

    for (std::size_t left = 0; left < expectedHashes.size(); ++left)
        for (std::size_t right = left + 1; right < expectedHashes.size(); ++right)
            if (expectedHashes[left].isEmpty()
                || expectedHashes[left] == expectedHashes[right])
                distinct = false;

    check (distinct, "the eight restored sounds stay distinct");

    checkFrequencies (session, check);
}

} // namespace ConcurrentInstances
