#include "api/discovery/MdnsSocket.h"

#include <cerrno>
#include <cstring>

#if ! defined (_WIN32)
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace Api::discovery
{

namespace
{
constexpr const char* multicastAddress = "224.0.0.251";
constexpr std::uint16_t multicastPort = 5353;

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
#endif
} // namespace

#if ! defined (_WIN32)
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
#else
int createMdnsSocket (const std::string&, std::string& error)
{
    error = "mDNS discovery is not implemented on this platform";
    return -1;
}

void sendPacket (int, const std::vector<std::uint8_t>&) {}
#endif

} // namespace Api::discovery
