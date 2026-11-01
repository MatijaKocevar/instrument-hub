#include "Socket.h"

#include <cstdint>

#if ! defined (_WIN32)
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace ApiTest::Discovery
{

#if ! defined (_WIN32)

namespace
{

constexpr const char* multicastAddress = "224.0.0.251";
constexpr std::uint16_t multicastPort = 5353;

} // namespace

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

void closeQuerySocket (int fd)
{
    ::close (fd);
}

#endif

} // namespace ApiTest::Discovery
