#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Api
{

    struct DiscoveryService
    {
        std::string instanceName = "Instrument Hub";

        std::string hostLabel;

        std::string address;

        int port = 8080;

        std::vector<std::pair<std::string, std::string>> txt{{"path", "/remote"},
                                                             {"version", "0.1.0"}};
    };

    class Discovery
    {
    public:
        Discovery ();
        ~Discovery ();

        Discovery (const Discovery&) = delete;
        Discovery& operator= (const Discovery&) = delete;

        static constexpr const char* serviceType = "_instrumenthub._tcp";
        static constexpr const char* defaultHostPrefix = "instrument-hub";

        bool start (DiscoveryService service);
        void stop ();

        bool isRunning () const noexcept;

        DiscoveryService getService () const;

        std::string getLastError () const;

        static std::string getServiceType ();

        static std::string sanitiseLabel (const std::string& text, const std::string& fallback);

        static std::string defaultHostLabel ();

    private:
        void stopLocked ();

        void run (int socketFd, DiscoveryService service);

        mutable std::mutex lifecycleMutex;
        mutable std::mutex stateMutex;

        DiscoveryService advertised;
        std::string lastError;

        std::thread worker;
        std::atomic<bool> running{false};
    };

}
