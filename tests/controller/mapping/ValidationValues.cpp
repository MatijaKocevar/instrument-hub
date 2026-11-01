#include "ValidationSupport.h"

#include <string>

namespace MappingTest
{

void runValueChecks()
{
    expectInvalid ("a macro with an inverted range", R"(schemaVersion: 1
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
    min: 1.0
    max: 0.0
pads: []
encoders: []
)", "min < max");

    expectInvalid ("a non-finite macro range", R"(schemaVersion: 1
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
    min: 0.0
    max: nan
pads: []
encoders: []
)", "finite number");

    expectInvalid ("an out-of-range patch colour", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
        colorIndex: 99
macros: {}
pads: []
encoders: []
)", "colorIndex");

    expectInvalid ("an out-of-range patch pan", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
        pan: 2.0
macros: {}
pads: []
encoders: []
)", "pan");

    expectInvalid ("an unknown FX field", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
        fx:
          - id: eq
            wet: 0.5
macros: {}
pads: []
encoders: []
)", "unknown key 'wet'");

    expectInvalid ("an unknown patch kind", R"(schemaVersion: 1
families:
  keys:
    name: Keys
    instruments:
      - id: piano
        name: Piano
        slot: 0
        kind: hologram
macros: {}
pads: []
encoders: []
)", "patch kind");
}

}
