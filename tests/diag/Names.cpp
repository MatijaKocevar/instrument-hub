#include "Names.h"
#include "TestSupport.h"

#include "diag/LogRing.h"

#include <iostream>
#include <string>

namespace DiagTest
{

void runLevelAndSourceNames()
{
    std::cout << "\n-- level and source parsing --" << std::endl;

    ihub::diag::LogLevel level = ihub::diag::LogLevel::info;
    ihub::diag::LogSource source = ihub::diag::LogSource::general;

    check (ihub::diag::parseLogLevel ("warning", level)
               && level == ihub::diag::LogLevel::warning,
           "warning parses case-insensitively");
    check (ihub::diag::parseLogSource ("PLUGINS", source)
               && source == ihub::diag::LogSource::plugins,
           "source names parse case-insensitively");
    check (! ihub::diag::parseLogLevel ("nonsense", level),
           "an unknown level is rejected");
    check (std::string (ihub::diag::toString (ihub::diag::LogLevel::fatal)) == "FATAL",
           "levels have stable text");
    check (std::string (ihub::diag::toString (ihub::diag::LogSource::audio)) == "audio",
           "sources have stable text");
}

} // namespace DiagTest
