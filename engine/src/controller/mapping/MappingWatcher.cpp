#include "controller/Mapping.h"

#include <chrono>
#include <system_error>

namespace Controller
{

    void Mapping::watchLoop (int intervalMs)
    {
        while (!quitWatcher.load (std::memory_order_acquire))
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (intervalMs));

            if (quitWatcher.load (std::memory_order_acquire))
                break;

            poll ();
        }

        watching.store (false, std::memory_order_release);
    }

    bool Mapping::startWatching (int intervalMs)
    {
        if (intervalMs < 5 || intervalMs > 5000)
            return false;

        const std::lock_guard<std::mutex> lock (watchMutex);

        if (watcher.joinable ())
            return true;

        quitWatcher.store (false, std::memory_order_release);
        watching.store (true, std::memory_order_release);

        try
        {
            watcher = std::thread ([this, intervalMs] { watchLoop (intervalMs); });
        }
        catch (const std::system_error&)
        {
            watching.store (false, std::memory_order_release);
            return false;
        }

        return true;
    }

    void Mapping::stopWatching ()
    {
        quitWatcher.store (true, std::memory_order_release);

        const std::lock_guard<std::mutex> lock (watchMutex);

        if (!watcher.joinable ())
        {
            watching.store (false, std::memory_order_release);
            return;
        }

        if (watcher.get_id () == std::this_thread::get_id ())
            return;

        watcher.join ();
        watching.store (false, std::memory_order_release);
    }

    bool Mapping::isWatching () const noexcept
    {
        return watching.load (std::memory_order_acquire);
    }

}
