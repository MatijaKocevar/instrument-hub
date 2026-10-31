#include "Discovery.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstring>
#include <vector>

#if ! defined (_WIN32)
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace Api
{

namespace
{
constexpr std::uint16_t dnsTypeA = 1;
constexpr std::uint16_t dnsTypePTR = 12;
constexpr std::uint16_t dnsTypeTXT = 16;
constexpr std::uint16_t dnsTypeAAAA = 28;
constexpr std::uint16_t dnsTypeSRV = 33;
constexpr std::uint16_t dnsTypeNSEC = 47;
constexpr std::uint16_t dnsTypeANY = 255;

constexpr std::uint16_t dnsClassIN = 1;
constexpr std::uint16_t dnsCacheFlush = 0x8000;

constexpr std::uint32_t ttlService = 4500;
constexpr std::uint32_t ttlHost = 120;

constexpr const char* multicastAddress = "224.0.0.251";
constexpr std::uint16_t multicastPort = 5353;

using Labels = std::vector<std::string>;

void appendU16 (std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
    out.push_back (static_cast<std::uint8_t> (value & 0xff));
}

void appendU32 (std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back (static_cast<std::uint8_t> ((value >> 24) & 0xff));
    out.push_back (static_cast<std::uint8_t> ((value >> 16) & 0xff));
    out.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
    out.push_back (static_cast<std::uint8_t> (value & 0xff));
}

std::uint16_t readU16 (const std::uint8_t* data)
{
    return static_cast<std::uint16_t> ((static_cast<std::uint16_t> (data[0]) << 8)
                                       | static_cast<std::uint16_t> (data[1]));
}

void appendName (std::vector<std::uint8_t>& out, const Labels& labels)
{
    for (const auto& label : labels)
    {
        const auto length = std::min<std::size_t> (label.size(), 63);
        out.push_back (static_cast<std::uint8_t> (length));
        out.insert (out.end(), label.begin(),
                    label.begin() + static_cast<std::ptrdiff_t> (length));
    }

    out.push_back (0);
}

/** Reads a possibly-compressed DNS name. `offset` is advanced past it. */
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

        if ((length & 0xc0) != 0 || position + 1 + length > size)
            return false;

        labels.emplace_back (reinterpret_cast<const char*> (data + position + 1),
                             static_cast<std::size_t> (length));
        position += 1 + static_cast<std::size_t> (length);

        if (labels.size() > 128)
            return false;
    }

    offset = jumped ? resumeAt : position;
    return true;
}

bool equalLabels (const Labels& left, const Labels& right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (left[index].size() != right[index].size())
            return false;

        for (std::size_t character = 0; character < left[index].size(); ++character)
        {
            const auto lower = [] (unsigned char c)
            {
                return static_cast<char> (std::tolower (c));
            };

            if (lower (static_cast<unsigned char> (left[index][character]))
                != lower (static_cast<unsigned char> (right[index][character])))
            {
                return false;
            }
        }
    }

    return true;
}

Labels serviceLabels()
{
    return { "_instrumenthub", "_tcp", "local" };
}

Labels enumerationLabels()
{
    return { "_services", "_dns-sd", "_udp", "local" };
}

Labels instanceLabels (const DiscoveryService& service)
{
    return { service.instanceName, "_instrumenthub", "_tcp", "local" };
}

Labels hostLabels (const DiscoveryService& service)
{
    return { service.hostLabel, "local" };
}

std::vector<std::uint8_t> encodeTxt (const DiscoveryService& service)
{
    std::vector<std::uint8_t> result;

    for (const auto& entry : service.txt)
    {
        auto text = entry.first.empty() ? entry.second
                                        : entry.first + "=" + entry.second;

        if (text.size() > 255)
            text.resize (255);

        result.push_back (static_cast<std::uint8_t> (text.size()));
        result.insert (result.end(), text.begin(), text.end());
    }

    // An empty TXT record still needs one zero-length string.
    if (result.empty())
        result.push_back (0);

    return result;
}

bool parseIpv4 (const std::string& text, std::array<std::uint8_t, 4>& address)
{
    std::size_t part = 0;
    std::size_t value = 0;
    bool haveDigit = false;

    for (std::size_t index = 0; index <= text.size(); ++index)
    {
        const bool end = index == text.size();
        const auto character = end ? '.' : text[index];

        if (character == '.')
        {
            if (! haveDigit || part >= 4 || value > 255)
                return false;

            address[part++] = static_cast<std::uint8_t> (value);
            value = 0;
            haveDigit = false;

            if (end && part != 4)
                return false;
        }
        else if (character >= '0' && character <= '9')
        {
            value = value * 10 + static_cast<std::size_t> (character - '0');
            haveDigit = true;
        }
        else
        {
            return false;
        }
    }

    return part == 4;
}

struct ResourceRecord
{
    Labels name;
    std::uint16_t type = 0;
    std::uint16_t klass = dnsClassIN;
    std::uint32_t ttl = 0;
    std::vector<std::uint8_t> rdata;
};

struct RecordSet
{
    ResourceRecord servicePtr;
    ResourceRecord enumerationPtr;
    ResourceRecord srv;
    ResourceRecord txt;
    ResourceRecord address;
    ResourceRecord nsec;
    bool hasAddress = false;
};

RecordSet makeRecordSet (const DiscoveryService& service, bool goodbye)
{
    const auto serviceTtl = goodbye ? 0u : ttlService;
    const auto hostTtl = goodbye ? 0u : ttlHost;
    const auto instance = instanceLabels (service);
    const auto host = hostLabels (service);

    RecordSet set;

    set.servicePtr.name = serviceLabels();
    set.servicePtr.type = dnsTypePTR;
    set.servicePtr.ttl = serviceTtl;
    appendName (set.servicePtr.rdata, instance);

    set.enumerationPtr.name = enumerationLabels();
    set.enumerationPtr.type = dnsTypePTR;
    set.enumerationPtr.ttl = serviceTtl;
    appendName (set.enumerationPtr.rdata, serviceLabels());

    set.srv.name = instance;
    set.srv.type = dnsTypeSRV;
    set.srv.klass = static_cast<std::uint16_t> (dnsClassIN | dnsCacheFlush);
    set.srv.ttl = hostTtl;
    appendU16 (set.srv.rdata, 0); // priority
    appendU16 (set.srv.rdata, 0); // weight
    appendU16 (set.srv.rdata, static_cast<std::uint16_t> (service.port));
    appendName (set.srv.rdata, host);

    set.txt.name = instance;
    set.txt.type = dnsTypeTXT;
    set.txt.klass = static_cast<std::uint16_t> (dnsClassIN | dnsCacheFlush);
    set.txt.ttl = serviceTtl;
    set.txt.rdata = encodeTxt (service);

    std::array<std::uint8_t, 4> ip {};

    if (parseIpv4 (service.address, ip) && ip[0] != 127)
    {
        set.address.name = host;
        set.address.type = dnsTypeA;
        set.address.klass = static_cast<std::uint16_t> (dnsClassIN | dnsCacheFlush);
        set.address.ttl = hostTtl;
        set.address.rdata.assign (ip.begin(), ip.end());
        set.hasAddress = true;

        // RFC 6762 §6.1: deny the record types this host does not have, so a
        // resolver asking for AAAA completes instead of waiting for an answer
        // that will never come. The bitmap lists A only (window 0, 4 bytes).
        set.nsec.name = host;
        set.nsec.type = dnsTypeNSEC;
        set.nsec.klass = static_cast<std::uint16_t> (dnsClassIN | dnsCacheFlush);
        set.nsec.ttl = hostTtl;
        appendName (set.nsec.rdata, host);
        set.nsec.rdata.insert (set.nsec.rdata.end(), { 0x00, 0x04, 0x40, 0x00, 0x00, 0x00 });
    }

    return set;
}

void appendRecord (std::vector<std::uint8_t>& packet, const ResourceRecord& record)
{
    appendName (packet, record.name);
    appendU16 (packet, record.type);
    appendU16 (packet, record.klass);
    appendU32 (packet, record.ttl);
    appendU16 (packet, static_cast<std::uint16_t> (record.rdata.size()));
    packet.insert (packet.end(), record.rdata.begin(), record.rdata.end());
}

std::vector<std::uint8_t> buildResponse (const DiscoveryService& service,
                                         bool goodbye)
{
    const auto records = makeRecordSet (service, goodbye);

    std::vector<std::uint8_t> packet;
    packet.reserve (512);

    appendU16 (packet, 0);          // multicast responses use ID 0
    appendU16 (packet, 0x8400);     // response + authoritative
    appendU16 (packet, 0);          // questions
    appendU16 (packet, 2);          // answers (service PTR + type enumeration)
    appendU16 (packet, 0);          // authority
    appendU16 (packet, records.hasAddress ? 4 : 2); // additional: SRV + TXT [+ A + NSEC]

    appendRecord (packet, records.servicePtr);
    appendRecord (packet, records.enumerationPtr);
    appendRecord (packet, records.srv);
    appendRecord (packet, records.txt);

    if (records.hasAddress)
    {
        appendRecord (packet, records.address);
        appendRecord (packet, records.nsec);
    }

    return packet;
}

bool isRecordOfInterest (const Labels& name, std::uint16_t type,
                         const DiscoveryService& service)
{
    const auto serviceName = serviceLabels();
    const auto instance = instanceLabels (service);
    const auto host = hostLabels (service);
    const auto enumeration = enumerationLabels();

    if (type == dnsTypeANY)
    {
        return equalLabels (name, serviceName)
               || equalLabels (name, instance)
               || equalLabels (name, host)
               || equalLabels (name, enumeration);
    }

    if (equalLabels (name, serviceName))
        return type == dnsTypePTR;

    if (equalLabels (name, instance))
        return type == dnsTypeSRV || type == dnsTypeTXT;

    if (equalLabels (name, host))
        return type == dnsTypeA || type == dnsTypeAAAA;

    if (equalLabels (name, enumeration))
        return type == dnsTypePTR;

    return false;
}

/** True when a received datagram is a query for one of our records. */
bool wantsResponse (const std::uint8_t* data, std::size_t size,
                    const DiscoveryService& service)
{
    if (size < 12)
        return false;

    const auto flags = readU16 (data + 2);

    if ((flags & 0x8000) != 0) // a response, not a query
        return false;

    const auto questions = readU16 (data + 4);
    std::size_t offset = 12;

    for (std::uint16_t index = 0; index < questions; ++index)
    {
        Labels name;

        if (! readName (data, size, offset, name) || offset + 4 > size)
            return false;

        const auto type = readU16 (data + offset);
        offset += 4;

        if (isRecordOfInterest (name, type, service))
            return true;
    }

    return false;
}

#if ! defined (_WIN32)

std::string socketError (const std::string& what)
{
    return what + ": " + std::strerror (errno);
}

void setSocketOption (int fd, int level, int option, const void* value,
                      socklen_t length)
{
    ::setsockopt (fd, level, option, value, length);
}

void setMulticastInterface (int fd, const std::string& address)
{
    in_addr interface {};

    if (address.empty() || ::inet_pton (AF_INET, address.c_str(), &interface) != 1)
        return;

    if (interface.s_addr == htonl (INADDR_LOOPBACK))
        return;

    setSocketOption (fd, IPPROTO_IP, IP_MULTICAST_IF, &interface, sizeof (interface));
}

/** Creates the shared mDNS socket, or returns -1 with `error` set. */
int createMdnsSocket (const std::string& address, std::string& error)
{
    const int fd = ::socket (AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        error = socketError ("cannot create the mDNS socket");
        return -1;
    }

    const int yes = 1;
    setSocketOption (fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes));

    #if defined (SO_REUSEPORT)
    setSocketOption (fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof (yes));
    #endif

    #if defined (FD_CLOEXEC)
    ::fcntl (fd, F_SETFD, FD_CLOEXEC);
    #endif

    sockaddr_in local {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl (INADDR_ANY);
    local.sin_port = htons (multicastPort);

    if (::bind (fd, reinterpret_cast<sockaddr*> (&local), sizeof (local)) != 0)
    {
        error = socketError ("cannot bind the mDNS socket to port 5353");
        ::close (fd);
        return -1;
    }

    ip_mreq membership {};
    membership.imr_multiaddr.s_addr = ::inet_addr (multicastAddress);
    membership.imr_interface.s_addr = htonl (INADDR_ANY);

    if (::setsockopt (fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership,
                      sizeof (membership)) != 0)
    {
        error = socketError ("cannot join " + std::string (multicastAddress));
        ::close (fd);
        return -1;
    }

    const unsigned char ttl = 255;
    setSocketOption (fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof (ttl));

    const unsigned char loop = 1;
    setSocketOption (fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof (loop));

    setMulticastInterface (fd, address);

    return fd;
}

void sendPacket (int fd, const std::vector<std::uint8_t>& packet)
{
    sockaddr_in destination {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons (multicastPort);
    destination.sin_addr.s_addr = ::inet_addr (multicastAddress);

    ::sendto (fd, packet.data(), packet.size(), 0,
              reinterpret_cast<sockaddr*> (&destination), sizeof (destination));
}

#endif // ! defined (_WIN32)
} // namespace

//==============================================================================
Discovery::Discovery() = default;

Discovery::~Discovery()
{
    stop();
}

bool Discovery::start (DiscoveryService service)
{
    const std::lock_guard<std::mutex> life (lifecycleMutex);
    stopLocked();

    if (service.instanceName.empty())
        service.instanceName = DiscoveryService{}.instanceName;

    if (service.instanceName.size() > 63)
        service.instanceName.resize (63);

    service.hostLabel = service.hostLabel.empty()
                            ? defaultHostLabel()
                            : sanitiseLabel (service.hostLabel, defaultHostLabel());

    std::string error;

    if (service.port <= 0 || service.port > 65535)
    {
        error = "invalid HTTP port " + std::to_string (service.port);
    }

    int fd = -1;

#if ! defined (_WIN32)
    if (error.empty())
        fd = createMdnsSocket (service.address, error);
#else
    error = "mDNS discovery is not implemented on this platform";
#endif

    if (fd < 0)
    {
        const std::lock_guard<std::mutex> lock (stateMutex);
        lastError = error;
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (stateMutex);
        advertised = service;
        lastError.clear();
    }

    running.store (true, std::memory_order_release);
    worker = std::thread ([this, fd, service]
    {
        run (fd, service);
    });

    return true;
}

void Discovery::stop()
{
    const std::lock_guard<std::mutex> life (lifecycleMutex);
    stopLocked();
}

void Discovery::stopLocked()
{
    running.store (false, std::memory_order_release);

    if (worker.joinable())
        worker.join();
}

bool Discovery::isRunning() const noexcept
{
    return running.load (std::memory_order_acquire);
}

DiscoveryService Discovery::getService() const
{
    const std::lock_guard<std::mutex> lock (stateMutex);
    return advertised;
}

std::string Discovery::getLastError() const
{
    const std::lock_guard<std::mutex> lock (stateMutex);
    return lastError;
}

void Discovery::run (int socketFd, DiscoveryService service)
{
#if ! defined (_WIN32)
    auto nextAnnouncement = std::chrono::steady_clock::now();
    int announcements = 0;
    std::uint8_t buffer[9000];

    while (running.load (std::memory_order_acquire))
    {
        pollfd descriptor {};
        descriptor.fd = socketFd;
        descriptor.events = POLLIN;

        const int ready = ::poll (&descriptor, 1, 200);

        if (ready > 0 && (descriptor.revents & POLLIN) != 0)
        {
            for (;;)
            {
                sockaddr_in from {};
                socklen_t fromLength = sizeof (from);

                const auto received = ::recvfrom (socketFd, buffer, sizeof (buffer),
                                                  MSG_DONTWAIT,
                                                  reinterpret_cast<sockaddr*> (&from),
                                                  &fromLength);

                if (received <= 0)
                    break;

                if (running.load (std::memory_order_acquire)
                    && wantsResponse (buffer, static_cast<std::size_t> (received), service))
                {
                    sendPacket (socketFd, buildResponse (service, false));
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();

        if (now >= nextAnnouncement)
        {
            sendPacket (socketFd, buildResponse (service, false));

            ++announcements;
            nextAnnouncement = now + (announcements < 2
                                          ? std::chrono::seconds (1)
                                          : std::chrono::seconds (60));
        }
    }

    // Goodbye: TTL 0 asks browsers to drop the instance immediately.
    sendPacket (socketFd, buildResponse (service, true));

    ::close (socketFd);
#else
    juce::ignoreUnused (socketFd, service);
#endif
}

//==============================================================================
std::string Discovery::getServiceType()
{
    return serviceType;
}

std::string Discovery::sanitiseLabel (const std::string& text,
                                      const std::string& fallback)
{
    std::string result;
    bool lastWasHyphen = false;

    for (const auto character : text)
    {
        const auto lower = static_cast<char> (
            std::tolower (static_cast<unsigned char> (character)));

        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9'))
        {
            result.push_back (lower);
            lastWasHyphen = false;
        }
        else if (! result.empty() && ! lastWasHyphen)
        {
            result.push_back ('-');
            lastWasHyphen = true;
        }
    }

    while (! result.empty() && result.back() == '-')
        result.pop_back();

    if (result.size() > 63)
        result.resize (63);

    while (! result.empty() && result.back() == '-')
        result.pop_back();

    return result.empty() ? fallback : result;
}

std::string Discovery::defaultHostLabel()
{
    const auto computer = juce::SystemStats::getComputerName().toStdString();
    const std::string prefix (defaultHostPrefix);

    return sanitiseLabel (computer.empty() ? prefix : prefix + "-" + computer,
                          prefix);
}

} // namespace Api
