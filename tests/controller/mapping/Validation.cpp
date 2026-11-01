#include "Validation.h"

#include "../TestSupport.h"

#include "Fixtures.h"
#include "ValidationSupport.h"
#include "controller/Mapping.h"

namespace MappingTest
{

using ControllerTest::check;

void runValidationChecks()
{
    {
        Controller::SetMapping parsed;
        Controller::MappingError error;

        check (Controller::parseSetMappingYaml (makeSetYaml ("sine-a"), parsed, error),
               "the generated two-instrument document parses");

        const auto* pad = parsed.padBinding (0);
        check (pad != nullptr && pad->slot == 0 && pad->instrument == "sine-a",
               "generated document resolves pad 0 to sine-a / slot 0");

        Controller::SetMapping emptyBindings;

        check (Controller::parseSetMappingYaml (R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros: {}
pads: []
encoders: []
)", emptyBindings, error),
               "a document with empty pads/encoders and macros parses");

        check (emptyBindings.countPads() == 0 && emptyBindings.countEncoders() == 0,
               "the empty bindings document binds nothing");
    }

    runStructureChecks();
    runValueChecks();
}

} // namespace MappingTest
