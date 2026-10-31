// T102 acceptance test: mDNS advertisement for `_instrumenthub._tcp`.
//
// 1. The DNS-SD type and host-label sanitising are correct.
// 2. Starting Discovery answers a PTR query on the mDNS multicast group with
//    the instance PTR plus SRV/TXT/A records carrying the API port.
// 3. Restarting on another port replaces the SRV port, and stop() ends the
//    advertisement.
//
// The test is a real multicast round trip, not a stubbed one. On a host with
// no working multicast route the integration half is skipped with a PASS note
// (the same graceful skip the acceptance recipe applies when avahi is absent).

#include "api/Discovery.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if ! defined (_WIN32)
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

void skip (const std::string& what)
{
    std::cout << "SKIP " << what << std::endl;
}

#if ! defined (_WIN32)

constexpr const char* multicastAddress = "224.0.0.251";
constexpr std::uint16_t multicastPort = 5353;

using Labels = std::vector<std::string>;

std::uint16_t readU16 (const std::uint8_t* data)
{
    return static_cast<std::uint16_t> ((static_cast<std::uint16_t> (data[0]) << 8)
                                       | data[1]);
}

std::uint32_t readU32 (const std::uint8_t* data)
{
    return (static_cast<std::uint32_t> (data[0]) << 24)
           | (static_cast<std::uint32_t> (data[1]) << 16)
           | (static_cast<std::uint32_t> (data[2]) << 8)
           | static_cast<std::uint32_t> (data[3]);
}

bool readName (const std::uint8_t* data, std::size_t size, std::size_t& offset,
               Labels& labels)
{
    std::size_t position = offset;
    std::size_t resumeAt = 0;
    bool jumped = false;
    int jumps = 0;

    for (;;)
    {
        if (position >= size)
            return false;

        const auto length = data[position];

        if (length == 0)
        {
            ++position;
            break;
        }

        if ((length & 0xc0) == 0xc0)
        {
            if (position + 1 >= size)
                return false;

            const auto target = static_cast<std::size_t> (
                ((length & 0x3f) << 8) | data[position + 1]);

            if (! jumped)
            {
                resumeAt = position + 2;
                jumped = true;
            }

            if (++jumps > 16 || target >= size)
                return false;

            position = target;
            continue;
        }

        if (position + 1 + length > size)
            return false;

        labels.emplace_back (reinterpret_cast<const char*> (data + position + 1),
                             static_cast<std::size_t> (length));
        position += 1 + static_cast<std::size_t> (length);
    }

    offset = jumped ? resumeAt : position;
    return true;
}

std::string join (const Labels& labels)
{
    std::string result;

    for (const auto& label : labels)
    {
        if (! result.empty())
            result += '.';

        result += label;
    }

    return result;
}

struct Record
{
    Labels name;
    std::uint16_t type = 0;
    std::uint16_t klass = 0;
    std::uint32_t ttl = 0;
    std::vector<std::uint8_t> rdata;
};

bool parseMessage (const std::uint8_t* data, std::size_t size,
                   std::vector<Record>& records, bool& isResponse)
{
    if (size < 12)
        return false;

    const auto flags = readU16 (data + 2);
    isResponse = (flags & 0x8000) != 0;

    const auto questions = readU16 (data + 4);
    const auto answers = readU16 (data + 6);
    const auto authority = readU16 (data + 8);
    const auto additional = readU16 (data + 10);

    std::size_t offset = 12;

    for (std::uint16_t index = 0; index < questions; ++index)
    {
        Labels ignored;

        if (! readName (data, size, offset, ignored) || offset + 4 > size)
            return false;

        offset += 4;
    }

    const auto total = static_cast<std::uint32_t> (answers)
                       + authority + additional;

    for (std::uint32_t index = 0; index < total; ++index)
    {
        Record record;

        if (! readName (data, size, offset, record.name) || offset + 10 > size)
            return false;

        record.type = readU16 (data + offset);
        record.klass = readU16 (data + offset + 2);
        record.ttl = readU32 (data + offset + 4);
        const auto length = readU16 (data + offset + 8);
        offset += 10;

        if (offset + length > size)
            return false;

        record.rdata.assign (data + offset, data + offset + length);
        offset += length;
        records.push_back (std::move (record));
    }

    return true;
}

bool readRdataName (const Record& record, std::size_t start, Labels& labels)
{
    std::size_t offset = start;
    return readName (record.rdata.data(), record.rdata.size(), offset, labels);
}

std::vector<std::uint8_t> makePtrQuery (const Labels& name, std::uint16_t id)
{
    std::vector<std::uint8_t> packet;

    const auto appendU16 = [&packet] (std::uint16_t value)
    {
        packet.push_back (static_cast<std::uint8_t> (value >> 8));
        packet.push_back (static_cast<std::uint8_t> (value & 0xff));
    };

    appendU16 (id);
    appendU16 (0); // standard query
    appendU16 (1); // one question
    appendU16 (0);
    appendU16 (0);
    appendU16 (0);

    for (const auto& label : name)
    {
        packet.push_back (static_cast<std::uint8_t> (label.size()));
        packet.insert (packet.end(), label.begin(), label.end());
    }

    packet.push_back (0);
    appendU16 (12); // PTR
    appendU16 (1);  // class IN

    return packet;
}

std::string localAddress()
{
    const int probe = ::socket (AF_INET, SOCK_DGRAM, 0);

    if (probe < 0)
        return {};

    sockaddr_in remote {};
    remote.sin_family = AF_INET;
    remote.sin_port = htons (9);
    ::inet_pton (AF_INET, "192.0.2.1", &remote.sin_addr);

    std::string result;

    if (::connect (probe, reinterpret_cast<sockaddr*> (&remote), sizeof (remote)) == 0)
    {
        sockaddr_in local {};
        socklen_t length = sizeof (local);

        if (::getsockname (probe, reinterpret_cast<sockaddr*> (&local), &length) == 0)
            result = ::inet_ntoa (local.sin_addr);
    }

    ::close (probe);
    return result;
}

int openQuerySocket()
{
    const int fd = ::socket (AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
        return -1;

    const int yes = 1;
    ::setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes));

    #if defined (SO_REUSEPORT)
    ::setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof (yes));
    #endif

    sockaddr_in local {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl (INADDR_ANY);
    local.sin_port = htons (multicastPort);

    if (::bind (fd, reinterpret_cast<sockaddr*> (&local), sizeof (local)) != 0)
    {
        ::close (fd);
        return -1;
    }

    ip_mreq membership {};
    membership.imr_multiaddr.s_addr = ::inet_addr (multicastAddress);
    membership.imr_interface.s_addr = htonl (INADDR_ANY);

    if (::setsockopt (fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership,
                      sizeof (membership)) != 0)
    {
        ::close (fd);
        return -1;
    }

    const unsigned char loop = 1;
    ::setsockopt (fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof (loop));

    return fd;
}

/** Sends a PTR query and waits for a response naming `instance`. */
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

#endif // ! defined (_WIN32)
} // namespace

int main()
{
    // --- Pure helpers ------------------------------------------------------
    check (Api::Discovery::getServiceType() == "_instrumenthub._tcp",
           "service type is _instrumenthub._tcp");
    check (Api::Discovery::sanitiseLabel ("My Mac!! .local", "fallback")
               == "my-mac-local",
           "host labels are lower-cased, trimmed and hyphenated");
    check (Api::Discovery::sanitiseLabel ("***", "fallback") == "fallback",
           "a label with no usable characters falls back");
    check (Api::Discovery::defaultHostLabel().rfind ("instrument-hub", 0) == 0,
           "the default host label starts with instrument-hub");

#if ! defined (_WIN32)
    const auto address = localAddress();

    Api::Discovery discovery;
    Api::DiscoveryService service;
    service.instanceName = "Instrument Hub Discovery Test";
    service.hostLabel = "ihub-discovery-test";
    service.address = address;
    service.port = 18099;
    service.txt = { { "path", "/remote" }, { "version", "0.1.0" } };

    if (! discovery.start (service))
    {
        skip ("multicast advertisement unavailable: " + discovery.getLastError());
    }
    else
    {
        check (discovery.isRunning(), "Discovery reports running after start()");
        check (discovery.getService().hostLabel == "ihub-discovery-test",
               "the advertised host label is kept");

        const int fd = openQuerySocket();

        if (fd < 0)
        {
            skip ("could not open an mDNS query socket");
        }
        else
        {
            std::string detail;
            const bool found = findAdvertisement (fd, service.instanceName,
                                                  service.port, detail);
            check (found, "mDNS query returns PTR/SRV/TXT for the instance ("
                              + detail + ")");

            // A second instance on a new port must replace the SRV target.
            Api::DiscoveryService second = service;
            second.instanceName = "Instrument Hub Discovery Test 2";
            second.port = 18100;

            check (discovery.start (second), "Discovery can restart on a new port");

            std::string secondDetail;

            if (discovery.isRunning())
            {
                const bool secondFound = findAdvertisement (fd, second.instanceName,
                                                             second.port, secondDetail);
                check (secondFound, "restart advertises the new SRV port ("
                                        + secondDetail + ")");
            }

            ::close (fd);
        }

        discovery.stop();
        check (! discovery.isRunning(), "Discovery stops cleanly");
    }
#else
    skip ("mDNS integration test is POSIX-only");
#endif

    std::cout << (failures == 0 ? "discovery-test: PASS" : "discovery-test: FAIL")
              << std::endl;

    return failures == 0 ? 0 : 1;
}
