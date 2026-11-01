#include "api/MidiBridge.h"

#include "controller/LaunchpadDriver.h"

#include <chrono>
#include <thread>

namespace Api
{

namespace
{
constexpr const char* launchpadPadTestError =
    "the Launchpad is not connected; check that its MI port is visible";

std::string toStd (const juce::String& value)
{
    return value.toStdString();
}
}

bool MidiBridge::isPadTestActive() const noexcept
{
    return padTestActive.load (std::memory_order_acquire);
}

bool MidiBridge::runPadTest (std::string& error)
{
    auto* driver = launchpad.load (std::memory_order_acquire);

    if (driver == nullptr)
    {
        error = "the Launchpad driver is not attached";
        return false;
    }

    if (! driver->isConnected() && ! driver->canSendLeds())
    {
        error = toStd (driver->getLastError());

        if (error.empty())
            error = launchpadPadTestError;

        return false;
    }

    const std::lock_guard<std::mutex> lock (padTestMutex);

    if (padTestThread.joinable())
        padTestThread.join();

    if (shuttingDown.load (std::memory_order_acquire))
        return false;

    const auto generation = padTestGeneration.fetch_add (1, std::memory_order_acq_rel) + 1;
    padTestLit.store (Launchpad::padCount, std::memory_order_release);
    padTestActive.store (true, std::memory_order_release);

    paintPadTestFrame (0);

    padTestThread = std::thread ([this, generation]
    {
        runPadTestAnimation (generation);
    });

    return true;
}

void MidiBridge::paintPadTestFrame (int frame)
{
    auto* driver = launchpad.load (std::memory_order_acquire);

    if (driver == nullptr)
        return;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const int column = pad % 8;
        const bool armed = column == frame % 8;

        driver->setPadLed (pad, Controller::launchpadLedForPatch (column, armed, false, false));
    }
}

void MidiBridge::runPadTestAnimation (std::uint64_t generation)
{
    for (int frame = 1; frame < 8; ++frame)
    {
        if (shuttingDown.load (std::memory_order_acquire)
            || padTestGeneration.load (std::memory_order_acquire) != generation)
            return;

        std::this_thread::sleep_for (std::chrono::milliseconds (padTestDurationMs / 8));

        if (shuttingDown.load (std::memory_order_acquire)
            || padTestGeneration.load (std::memory_order_acquire) != generation)
            return;

        paintPadTestFrame (frame);
    }

    if (auto* driver = launchpad.load (std::memory_order_acquire);
        driver != nullptr && padTestGeneration.load (std::memory_order_acquire) == generation)
    {
        for (int pad = 0; pad < Launchpad::padCount; ++pad)
            driver->setPadLed (pad, {});
    }

    padTestLit.store (0, std::memory_order_release);
    padTestActive.store (false, std::memory_order_release);

    if (shuttingDown.load (std::memory_order_acquire))
        return;

    RestoreHandler restore;

    {
        const std::lock_guard<std::mutex> lock (restoreMutex);
        restore = restoreHandler;
    }

    if (restore)
        restore();
}

}
