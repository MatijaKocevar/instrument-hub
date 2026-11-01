#include "Records.h"
#include "TestSupport.h"

#include "diag/LogRing.h"

#include <cstring>
#include <iostream>
#include <string>

namespace DiagTest
{

void runStructuredRecords()
{
    std::cout << "\n-- structured records --" << std::endl;

    ihub::diag::LogRing ring (4);

    ring.log (ihub::diag::LogLevel::warning,
              ihub::diag::LogSource::audio,
              42,
              "buffer underrun");

    ihub::diag::LogRecord records[4] {};
    auto count = ring.copyLatest (records, 4);

    check (count == 1, "one record is readable");
    check (records[0].sequence == 0, "the ring assigns sequence numbers from 0");
    check (records[0].level == ihub::diag::LogLevel::warning, "the level is stored");
    check (records[0].source == ihub::diag::LogSource::audio, "the source is stored");
    check (records[0].code == 42, "the structured code is stored");
    check (std::string (records[0].message) == "buffer underrun", "the message is stored");
    check (records[0].timestampMs > 0, "the record has a wall-clock timestamp");

    for (int index = 0; index < 12; ++index)
        ring.log (ihub::diag::LogLevel::info,
                  ihub::diag::LogSource::general,
                  index,
                  "record " + std::to_string (index));

    count = ring.copyLatest (records, 4);

    check (count == 4, "a full ring returns its capacity");
    check (records[0].code == 8 && records[3].code == 11,
           "the newest four records are returned, oldest first");
    check (records[0].sequence == 9 && records[3].sequence == 12,
           "returned records keep their monotonic sequence numbers");
    check (ring.getPushedCount() == 13, "every record is counted");
    check (ring.getOverwrittenCount() == 9,
           "records dropped by the wrap are reported as overwritten");

    ihub::diag::LogRecord two[2] {};
    check (ring.copyLatest (two, 2) == 2 && two[0].code == 10 && two[1].code == 11,
           "a small destination receives the newest records");

    ring.log (ihub::diag::LogLevel::error,
              ihub::diag::LogSource::diagnostics,
              7,
              std::string (ihub::diag::logMessageCapacity * 4, 'x'));

    ihub::diag::LogRecord longest {};
    ring.copyLatest (&longest, 1);

    check (longest.message[ihub::diag::logMessageCapacity - 1] == '\0',
           "a long message is terminated inside the record");
    check (std::strlen (longest.message) == ihub::diag::logMessageCapacity - 1,
           "a long message is truncated to the record capacity");
}

} // namespace DiagTest
