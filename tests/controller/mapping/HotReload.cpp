#include "HotReload.h"

#include "../TestSupport.h"

#include "Fixtures.h"
#include "Render.h"
#include "controller/Mapping.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace MappingTest
{

using ControllerTest::check;

void runHotReloadChecks()
{
    const auto setFile = std::filesystem::current_path() / "mapping-hot-reload.yaml";
    writeFileAtomic (setFile, makeSetYaml ("sine-a"));

    std::unique_ptr<SineSource> sourceA = std::make_unique<SineSource> (220.0, 0.2, 0.0);
    std::unique_ptr<SineSource> sourceB = std::make_unique<SineSource> (330.0, 0.2, 1.1);

    AudioEngine::Slot slot0 (0);
    AudioEngine::Slot slot1 (1);
    slot0.setSource (sourceA.get());
    slot1.setSource (sourceB.get());

    AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
    switcher.setSlots ({ &slot0, &slot1 });
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    Controller::Mapping mapping;

    std::atomic<bool> routedToNewSlot { false };
    std::atomic<double> routedAtMs { 0.0 };
    std::atomic<int> lastArmedSlot { -99 };
    std::atomic<int> hotReloadCount { 0 };
    std::atomic<int> errorCount { 0 };

    Controller::Mapping::Listeners listeners;

    listeners.armSlot = [&] (int targetSlot)
    {
        if (targetSlot >= 0)
            switcher.armSlot (targetSlot);
        else
            switcher.disarm();

        lastArmedSlot.store (targetSlot, std::memory_order_release);

        if (targetSlot == 1 && ! routedToNewSlot.exchange (true))
            routedAtMs.store (nowMs(), std::memory_order_release);
    };

    listeners.changed = [&] (const std::shared_ptr<const Controller::SetMapping>& document,
                             bool hotReload)
    {
        if (hotReload && document != nullptr)
            hotReloadCount.fetch_add (1);
    };

    listeners.error = [&] (const Controller::MappingError&)
    {
        errorCount.fetch_add (1);
    };

    mapping.setListeners (listeners);

    Controller::MappingError loadError;
    check (mapping.load (setFile, &loadError),
           "initial mapping loads" + (loadError.isEmpty()
                                          ? std::string()
                                          : " (" + loadError.message + ")"));
    check (mapping.getRevision() == 1, "initial load publishes revision 1");
    check (mapping.startWatching (20), "the file watcher starts");
    check (mapping.isWatching(), "isWatching() reports the watcher");

    check (mapping.handlePad (0, true), "pad 0 press is mapped");
    check (switcher.getArmedSlot() == 0, "pad 0 arms slot 0");
    check (mapping.getArmedSlot() == 0, "mapping reports slot 0 as armed");
    check (mapping.getActivePad() == 0, "mapping tracks pad 0 as the active pad");

    juce::MidiBuffer midi;
    RenderStats stats;
    float previousSample = 0.0f;

    for (int block = 0; block < kPreRollBlocks; ++block)
        renderBlock (switcher, midi, stats, previousSample);

    stats = RenderStats {};
    previousSample = switcher.getOutputBuffer().getSample (0, kBlockSize - 1);

    check (switcher.getSlotGain (0) > 0.99f, "slot 0 is fully active before the edit");

    const double editIssuedAtMs = nowMs();
    writeFileAtomic (setFile, makeSetYaml ("sine-b", "reverb"));

    const auto renderStart = std::chrono::steady_clock::now();
    const auto blockDuration = std::chrono::microseconds (2600);
    long long blocksRendered = 0;

    const auto renderAndPace = [&]
    {
        renderBlock (switcher, midi, stats, previousSample);
        ++blocksRendered;
        std::this_thread::sleep_until (renderStart + blockDuration * blocksRendered);
    };

    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (1200);

    while (std::chrono::steady_clock::now() < deadline)
    {
        renderAndPace();

        if (routedToNewSlot.load() && blocksRendered > 20)
            break;
    }

    const double reloadLatencyMs = routedAtMs.load() - editIssuedAtMs;

    check (routedToNewSlot.load(), "editing pad 0's instrument arms slot 1");
    check (reloadLatencyMs >= 0.0 && reloadLatencyMs < kHotReloadBudgetMs,
           "routing changed in " + std::to_string (reloadLatencyMs)
               + " ms (budget " + std::to_string (kHotReloadBudgetMs) + " ms)");
    check (switcher.getArmedSlot() == 1, "SlotSwitcher is armed on slot 1");
    check (mapping.getArmedSlot() == 1, "mapping reports slot 1 as armed");
    check (mapping.getRevision() == 2, "the reload published revision 2");
    check (hotReloadCount.load() == 1, "one hot reload was published");
    check (lastArmedSlot.load() == 1, "the arm listener received slot 1");

    check (switcher.getSlotGain (1) > 0.99f, "slot 1 reached full gain");
    check (switcher.getSlotGain (0) < 0.01f, "slot 0 faded out");
    check (stats.alwaysRendered, "every block rendered audio during the reload");
    check (stats.silentBlocks == 0,
           "no dropout: every block stayed above " + std::to_string (kDropoutRmsFloor)
               + " RMS (minimum " + std::to_string (stats.minRms) + ")");
    check (stats.maxDelta <= kDeltaThreshold,
           "no click: max sample delta " + std::to_string (stats.maxDelta)
               + " <= " + std::to_string (kDeltaThreshold));

    if (const auto document = mapping.getMapping())
    {
        const auto* binding = document->padBinding (0);
        check (binding != nullptr && binding->instrument == "sine-b" && binding->slot == 1,
               "the published snapshot binds pad 0 to sine-b / slot 1");
    }
    else
    {
        check (false, "getMapping() returns the published snapshot");
    }

    std::optional<Controller::Mapping::MacroChange> lastMacro;
    listeners.macroEvent = [&] (const Controller::Mapping::MacroChange& change)
    {
        lastMacro = change;
    };
    mapping.setListeners (listeners);
    check (mapping.handleEncoder (0, 127), "encoder 0 resolves after the reload");
    check (lastMacro.has_value() && lastMacro->macro.id == "reverb"
               && lastMacro->slot == 1 && std::abs (lastMacro->normalized - 1.0f) < 1.0e-6f
               && std::abs (lastMacro->value - 1.0f) < 1.0e-6f,
           "encoder 0 now drives reverb on slot 1 at full scale");

    const auto revisionBeforeBadEdit = mapping.getRevision();
    const int errorsBeforeBadEdit = errorCount.load();

    writeFileAtomic (setFile, "schemaVersion: 99\nfamilies: []\n");

    const auto badDeadline = std::chrono::steady_clock::now()
                             + std::chrono::milliseconds (600);

    while (std::chrono::steady_clock::now() < badDeadline
           && errorCount.load() == errorsBeforeBadEdit)
    {
        renderAndPace();
    }

    check (errorCount.load() > errorsBeforeBadEdit, "a malformed edit reports an error");
    check (mapping.getRevision() == revisionBeforeBadEdit,
           "a malformed edit does not publish a new document");
    check (mapping.getArmedSlot() == 1, "a malformed edit keeps slot 1 armed");
    check (stats.alwaysRendered && stats.silentBlocks == 0,
           "audio kept flowing across the malformed edit");

    if (const auto document = mapping.getMapping())
    {
        const auto* binding = document->padBinding (0);
        check (binding != nullptr && binding->instrument == "sine-b",
               "the previous pad binding is still live");
    }

    const auto revisionBeforeRecovery = mapping.getRevision();
    writeFileAtomic (setFile, makeSetYaml ("sine-a"));

    const auto recoveryDeadline = std::chrono::steady_clock::now()
                                  + std::chrono::milliseconds (600);

    while (std::chrono::steady_clock::now() < recoveryDeadline
           && mapping.getRevision() == revisionBeforeRecovery)
    {
        renderAndPace();
    }

    check (mapping.getRevision() == revisionBeforeRecovery + 1,
           "a valid edit after an error is published");
    check (mapping.getArmedSlot() == 0, "recovery re-arms the edited slot");
    check (mapping.getLastError().isEmpty(), "the error is cleared after recovery");

    mapping.stopWatching();
    check (! mapping.isWatching(), "stopWatching() stops the watcher");

    std::error_code ec;
    std::filesystem::remove (setFile, ec);
}

}
