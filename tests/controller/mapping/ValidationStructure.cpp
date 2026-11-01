#include "ValidationSupport.h"

#include "Fixtures.h"

#include <string>

namespace MappingTest
{

void runStructureChecks()
{
    auto futureSchemaYaml = makeSetYaml ("sine-a");
    const auto schemaPosition = futureSchemaYaml.find ("schemaVersion: 1");

    if (schemaPosition != std::string::npos)
        futureSchemaYaml.replace (schemaPosition, std::string ("schemaVersion: 1").size(),
                                  "schemaVersion: 2");

    expectInvalid ("a future schemaVersion", futureSchemaYaml, "unsupported schemaVersion");

    expectInvalid ("an unknown family", R"(schemaVersion: 1
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
    family: drums
    instrument: piano
encoders: []
)", "unknown family");

    expectInvalid ("an unknown instrument", R"(schemaVersion: 1
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
    instrument: strings
encoders: []
)", "unknown instrument");

    expectInvalid ("a duplicate pad", R"(schemaVersion: 1
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
  - pad: 0
    family: keys
    instrument: piano
encoders: []
)", "duplicate pad");

    expectInvalid ("a duplicate family key", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
  keys:
    name: Keys Again
    instruments:
      - id: organ
        name: Organ
        slot: 1
macros:
  cutoff:
    name: Cutoff
    parameter: filter.cutoff
pads: []
encoders: []
)", "duplicate key");

    expectInvalid ("tab indentation", "schemaVersion: 1\nfamilies:\n\tkeys:\n\t\tname: Keys\n",
                   "tab indentation");

    expectInvalid ("inconsistent indentation", R"(schemaVersion: 1
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
)", "unexpected indentation");

    expectInvalid ("a missing pads section", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros: {}
encoders: []
)", "pads is required");

    expectInvalid ("a missing encoders section", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
macros: {}
pads: []
)", "encoders is required");

    expectInvalid ("an empty families mapping", R"(schemaVersion: 1
families: {}
macros: {}
pads: []
encoders: []
)", "families must not be empty");
}

}
