#include "Helpers.h"

#include "../TestSupport.h"

#include "api/Discovery.h"

namespace ApiTest::Discovery
{

void runPureHelpers()
{
    check (Api::Discovery::getServiceType() == "_instrumenthub._tcp",
           "service type is _instrumenthub._tcp");
    check (Api::Discovery::sanitiseLabel ("My Mac!! .local", "fallback")
               == "my-mac-local",
           "host labels are lower-cased, trimmed and hyphenated");
    check (Api::Discovery::sanitiseLabel ("***", "fallback") == "fallback",
           "a label with no usable characters falls back");
    check (Api::Discovery::defaultHostLabel().rfind ("instrument-hub", 0) == 0,
           "the default host label starts with instrument-hub");
}

} // namespace ApiTest::Discovery
