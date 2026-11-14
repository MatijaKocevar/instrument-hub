#include "Discovery.h"

#include "discovery/DnsPacket.h"
#include "discovery/MdnsSocket.h"

#include <juce_core/juce_core.h>

#include <cctype>
#include <chrono>
#include <thread>
#include <utility>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Api
{

    Discovery::Discovery () = default;

    Discovery::~Discovery ()
    {
        stop ();
    }

    bool Discovery::start (DiscoveryService service)
    {
        const std::lock_guard<std::mutex> life (lifecycleMutex);
        stopLocked ();

        if (service.instanceName.empty ())
            service.instanceName = DiscoveryService{}.instanceName;

        if (service.instanceName.size () > 63)
            service.instanceName.resize (63);

        service.hostLabel = service.hostLabel.empty ()
                                ? defaultHostLabel ()
                                : sanitiseLabel (service.hostLabel, defaultHostLabel ());

        std::string error;

        if (service.port <= 0 || service.port > 65535)
        {
            error = "invalid HTTP port " + std::to_string (service.port);
        }

        int fd = -1;

        if (error.empty ())
            fd = discovery::createMdnsSocket (service.address, error);

        if (fd < 0)
        {
            const std::lock_guard<std::mutex> lock (stateMutex);
            lastError = error;
            return false;
        }

        {
            const std::lock_guard<std::mutex> lock (stateMutex);
            advertised = service;
            lastError.clear ();
        }

        running.store (true, std::memory_order_release);
        worker = std::thread ([this, fd, service] { run (fd, service); });

        return true;
    }

    void Discovery::stop ()
    {
        const std::lock_guard<std::mutex> life (lifecycleMutex);
        stopLocked ();
    }

    void Discovery::stopLocked ()
    {
        running.store (false, std::memory_order_release);

        if (worker.joinable ())
            worker.join ();
    }

    bool Discovery::isRunning () const noexcept
    {
        return running.load (std::memory_order_acquire);
    }

    DiscoveryService Discovery::getService () const
    {
        const std::lock_guard<std::mutex> lock (stateMutex);
        return advertised;
    }

    std::string Discovery::getLastError () const
    {
        const std::lock_guard<std::mutex> lock (stateMutex);
        return lastError;
    }

    void Discovery::run (int socketFd, DiscoveryService service)
    {
#if !defined(_WIN32)
        auto nextAnnouncement = std::chrono::steady_clock::now ();
        int announcements = 0;
        std::uint8_t buffer[9000];

        while (running.load (std::memory_order_acquire))
        {
            pollfd descriptor{};
            descriptor.fd = socketFd;
            descriptor.events = POLLIN;

            const int ready = ::poll (&descriptor, 1, 200);

            if (ready > 0 && (descriptor.revents & POLLIN) != 0)
            {
                for (;;)
                {
                    sockaddr_in from{};
                    socklen_t fromLength = sizeof (from);

                    const auto received = ::recvfrom (socketFd,
                                                      buffer,
                                                      sizeof (buffer),
                                                      MSG_DONTWAIT,
                                                      reinterpret_cast<sockaddr*> (&from),
                                                      &fromLength);

                    if (received <= 0)
                        break;

                    if (running.load (std::memory_order_acquire) &&
                        wantsResponse (buffer, static_cast<std::size_t> (received), service))
                    {
                        discovery::sendPacket (socketFd, buildResponse (service, false));
                    }
                }
            }

            const auto now = std::chrono::steady_clock::now ();

            if (now >= nextAnnouncement)
            {
                discovery::sendPacket (socketFd, buildResponse (service, false));

                ++announcements;
                nextAnnouncement = now + (announcements < 2 ? std::chrono::seconds (1)
                                                            : std::chrono::seconds (60));
            }
        }

        discovery::sendPacket (socketFd, buildResponse (service, true));

        ::close (socketFd);
#else
        juce::ignoreUnused (socketFd, service);
#endif
    }

    std::string Discovery::getServiceType ()
    {
        return serviceType;
    }

    std::string Discovery::sanitiseLabel (const std::string& text, const std::string& fallback)
    {
        std::string result;
        bool lastWasHyphen = false;

        for (const auto character : text)
        {
            const auto lower =
                static_cast<char> (std::tolower (static_cast<unsigned char> (character)));

            if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9'))
            {
                result.push_back (lower);
                lastWasHyphen = false;
            }
            else if (!result.empty () && !lastWasHyphen)
            {
                result.push_back ('-');
                lastWasHyphen = true;
            }
        }

        while (!result.empty () && result.back () == '-')
            result.pop_back ();

        if (result.size () > 63)
            result.resize (63);

        while (!result.empty () && result.back () == '-')
            result.pop_back ();

        return result.empty () ? fallback : result;
    }

    std::string Discovery::defaultHostLabel ()
    {
        const auto computer = juce::SystemStats::getComputerName ().toStdString ();
        const std::string prefix (defaultHostPrefix);

        return sanitiseLabel (computer.empty () ? prefix : prefix + "-" + computer, prefix);
    }

}
