#include "Advertisement.h"

#include "../TestSupport.h"
#include "Query.h"
#include "Socket.h"

#include "api/Discovery.h"

#include <iostream>
#include <string>

namespace ApiTest::Discovery
{

void runAdvertisement()
{
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

            closeQuerySocket (fd);
        }

        discovery.stop();
        check (! discovery.isRunning(), "Discovery stops cleanly");
    }
#else
    skip ("mDNS integration test is POSIX-only");
#endif
}

}
