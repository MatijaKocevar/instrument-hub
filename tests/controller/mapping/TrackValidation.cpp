#include "TrackValidation.h"

#include "../TestSupport.h"

#include "Fixtures.h"
#include "TrackFixtures.h"
#include "controller/Mapping.h"

#include <string>

namespace MappingTest
{

using ControllerTest::check;

namespace
{

void expectInvalidAtLine (const std::string& label,
                          const std::string& yaml,
                          const std::string& expectedFragment,
                          int expectedLine)
{
    Controller::SetMapping mapping;
    mapping.name = "keep me";

    Controller::MappingError error;
    const bool ok = Controller::parseSetMappingYaml (yaml, mapping, error);

    check (! ok, label + " is rejected");
    check (! ok && error.message.find (expectedFragment) != std::string::npos,
           label + " reports '" + expectedFragment + "' (got '"
               + error.message + "' at line " + std::to_string (error.line) + ")");
    check (! ok && error.line == expectedLine,
           label + " reports line " + std::to_string (expectedLine) + " (got "
               + std::to_string (error.line) + ")");
    check (mapping.name == "keep me",
           label + " leaves the previous document untouched");
}

std::string onePad (const std::string& pad)
{
    return makeTrackHeader() + "\npads:\n" + pad;
}

}

void runTrackValidationChecks()
{
    {
        auto yaml = onePad ("  - pad: 56\n"
                            "    role: launch\n"
                            "    track: track-1\n");

        expectInvalidAtLine ("an unknown pad role", yaml, "unknown pad role",
                             lineOf (yaml, "    role: launch"));
    }

    {
        auto yaml = onePad ("  - pad: 56\n"
                            "    role: track\n");

        expectInvalidAtLine ("a track pad without a track", yaml, "needs pad and track",
                             lineOf (yaml, "  - pad: 56"));
    }

    {
        auto yaml = onePad ("  - pad: 48\n"
                            "    role: fx\n"
                            "    track: track-1\n"
                            "    hot: 2\n");

        expectInvalidAtLine ("an out-of-range fx hot", yaml, "fx hot must be 0 or 1",
                             lineOf (yaml, "    hot: 2"));
    }

    {
        auto yaml = onePad ("  - pad: 0\n"
                            "    role: volume\n"
                            "    track: track-1\n"
                            "    step: 5\n");

        expectInvalidAtLine ("an out-of-range volume step", yaml,
                             "volume step must be within 0..4",
                             lineOf (yaml, "    step: 5"));
    }

    {
        auto yaml = onePad ("  - pad: 0\n"
                            "    role: track\n"
                            "    track: track-1\n");

        expectInvalidAtLine ("a track role outside row 7", yaml,
                             "track pads belong on row 7",
                             lineOf (yaml, "  - pad: 0"));
    }

    {
        auto yaml = onePad ("  - pad: 56\n"
                            "    role: track\n"
                            "    track: missing\n");

        expectInvalidAtLine ("a pad naming an unknown track", yaml, "unknown track",
                             lineOf (yaml, "    track: missing"));
    }

    {
        auto yaml = replaceOnce (makeTracksYaml(), "  - id: track-8\n", "  - id: track-1\n");

        expectInvalidAtLine ("a duplicate track id", yaml, "duplicate track id",
                             lineOf (yaml, "  - id: track-1\n    name: Track 8"));
    }

    {
        auto yaml = replaceOnce (makeTracksYaml(), "    slot: 7\n", "    slot: 0\n");

        expectInvalidAtLine ("a duplicate track slot", yaml, "duplicate track slot",
                             lineOfOccurrence (yaml, "    slot: 0\n", 2));
    }

    {
        auto yaml = replaceOnce (makeTracksYaml(),
                                 "  - id: track-8\n"
                                 "    name: Track 8\n"
                                 "    slot: 7\n"
                                 "    hotFx:\n"
                                 "      - delay\n",
                                 "");

        expectInvalidAtLine ("a tracks list without eight entries", yaml,
                             "tracks must declare exactly 8 entries",
                             lineOf (yaml, "  - id: track-1"));
    }

    {
        expectInvalidAtLine ("schemaVersion 2 without layout",
                             "schemaVersion: 2\npads: []\n",
                             "needs layout: tracks", 1);
    }

    {
        expectInvalidAtLine ("layout: tracks without tracks",
                             "schemaVersion: 2\nlayout: tracks\npads: []\n",
                             "tracks is required for layout: tracks", 1);
    }

    {
        auto yaml = replaceOnce (makeSetYaml ("sine-a"),
                                 "    family: keys\n",
                                 "    role: patch\n    family: keys\n");

        expectInvalidAtLine ("a role on a version 1 pad", yaml, "unknown key 'role'",
                             lineOf (yaml, "  - pad: 0"));
    }
}

}
