#include "ValidationSupport.h"

#include "../TestSupport.h"

#include "controller/Mapping.h"

#include <string>

namespace MappingTest
{

void expectInvalid (const std::string& label,
                    const std::string& yaml,
                    const std::string& expectedFragment)
{
    Controller::SetMapping mapping;
    mapping.name = "keep me";

    Controller::MappingError error;
    const bool ok = Controller::parseSetMappingYaml (yaml, mapping, error);

    ControllerTest::check (! ok, label + " is rejected");
    ControllerTest::check (! ok && error.message.find (expectedFragment) != std::string::npos,
                           label + " reports '" + expectedFragment + "' (got '"
                               + error.message + "' at line " + std::to_string (error.line) + ")");
    ControllerTest::check (mapping.name == "keep me",
                           label + " leaves the previous document untouched");
}

} // namespace MappingTest
