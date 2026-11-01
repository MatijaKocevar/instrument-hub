#include "Query.h"

#include "Dns.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if ! defined (_WIN32)
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <poll.h>
    #include <sys/socket.h>
#endif

namespace ApiTest::Discovery
{

#if ! defined (_WIN32)

namespace
{

constexpr const char* multicastAddress = "224.0.0.251";
constexpr std::uint16_t multicastPort = 5353;

} // namespace

bool findAdvertisement (int fd, const std::string& instance, int expectedPort,
                        std::string& detail)
{
    const auto packet = makePtrQuery ({ "_instrumenthub", "_tcp", "local" }, 0x4242);

    sockaddr_in destination {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons (multicastPort);
    destination.sin_addr.s_addr = ::inet_addr (multicastAddress);

    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (3000);
    auto nextQuery = std::chrono::steady_clock::now();
    std::uint8_t buffer[9000];

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto now = std::chrono::steady_clock::now();

        if (now >= nextQuery)
        {
            ::sendto (fd, packet.data(), packet.size(), 0,
                      reinterpret_cast<sockaddr*> (&destination), sizeof (destination));
            nextQuery = now + std::chrono::milliseconds (700);
        }

        pollfd descriptor {};
        descriptor.fd = fd;
        descriptor.events = POLLIN;

        if (::poll (&descriptor, 1, 100) <= 0)
            continue;

        for (;;)
        {
            const auto received = ::recvfrom (fd, buffer, sizeof (buffer), MSG_DONTWAIT,
                                              nullptr, nullptr);

            if (received <= 0)
                break;

            std::vector<Record> records;
            bool isResponse = false;

            if (! parseMessage (buffer, static_cast<std::size_t> (received),
                                records, isResponse)
                || ! isResponse)
            {
                continue;
            }

            bool listsInstance = false;

            for (const auto& record : records)
            {
                if (record.type != 12 || join (record.name) != "_instrumenthub._tcp.local")
                    continue;

                Labels target;

                if (readRdataName (record, 0, target)
                    && ! target.empty() && target[0] == instance)
                {
                    listsInstance = true;
                }
            }

            if (! listsInstance)
                continue;

            bool srvOk = false;
            bool txtOk = false;
            bool addressOk = false;
            std::string targetHost;

            for (const auto& record : records)
            {
                const auto name = join (record.name);

                if (record.type == 33 && name == instance + "._instrumenthub._tcp.local"
                    && record.rdata.size() > 6)
                {
                    const auto port = static_cast<int> (
                        (static_cast<std::uint16_t> (record.rdata[4]) << 8)
                        | record.rdata[5]);
                    Labels target;

                    if (port == expectedPort && readRdataName (record, 6, target))
                    {
                        srvOk = true;
                        targetHost = join (target);
                    }
                }
                else if (record.type == 16
                         && name == instance + "._instrumenthub._tcp.local")
                {
                    std::size_t offset = 0;

                    while (offset < record.rdata.size())
                    {
                        const auto length = record.rdata[offset++];

                        if (offset + length > record.rdata.size())
                            break;

                        const std::string entry (
                            reinterpret_cast<const char*> (record.rdata.data() + offset),
                            length);

                        if (entry == "path=/remote")
                            txtOk = true;

                        offset += length;
                    }
                }
                else if (record.type == 1 && record.rdata.size() == 4)
                {
                    addressOk = true;
                }
            }

            detail = "ptr=yes srv=" + std::string (srvOk ? "yes" : "no")
                     + " txt=" + std::string (txtOk ? "yes" : "no")
                     + " a=" + std::string (addressOk ? "yes" : "no")
                     + " target=" + targetHost;

            return srvOk && txtOk;
        }
    }

    detail = "no matching response within 3 s";
    return false;
}

#endif

} // namespace ApiTest::Discovery
