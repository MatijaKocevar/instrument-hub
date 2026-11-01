#include "Validation.h"

#include "../TestSupport.h"

#include "controller/Mapping.h"

#include <string>

namespace LaunchpadMappingTest
{

using ControllerTest::check;

void runValidationChecks()
{
    auto expectInvalid = [] (const std::string& label,
                             const std::string& yaml,
                             const std::string& fragment)
    {
        Controller::SetMapping parsed;
        Controller::MappingError parseError;
        const bool ok = Controller::parseSetMappingYaml (yaml, parsed, parseError);

        check (! ok, label + " is rejected");
        check (! ok && parseError.message.find (fragment) != std::string::npos,
               label + " reports '" + fragment + "' (got '"
                   + parseError.message + "')");
    };

    const std::string prefix = R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros:
  cutoff:
    name: Cutoff
    parameter: filter.cutoff
pads:
  - pad: 0
    family: keys
    instrument: piano
encoders: []
)";

    expectInvalid ("a duplicate bank", prefix + R"(banks:
  - bank: 0
    name: A
    macros:
      - cutoff
  - bank: 0
    name: B
    macros:
      - cutoff
scenes: []
)", "duplicate bank");

    expectInvalid ("a bank with an unknown macro", prefix + R"(banks:
  - bank: 0
    name: A
    macros:
      - missing
scenes: []
)", "unknown macro");

    expectInvalid ("a duplicate scene", prefix + R"(banks: []
scenes:
  - scene: 0
    name: A
    family: keys
  - scene: 0
    name: B
    family: keys
)", "duplicate scene");

    expectInvalid ("a scene with an unknown family", prefix + R"(banks: []
scenes:
  - scene: 0
    name: A
    family: drums
)", "unknown family");

    expectInvalid ("pad 64", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros:
  cutoff:
    name: Cutoff
    parameter: filter.cutoff
pads:
  - pad: 64
    family: keys
    instrument: piano
encoders: []
banks: []
scenes: []
)", "pad must be within");
}

}
