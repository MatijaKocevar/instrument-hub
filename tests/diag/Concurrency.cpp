#include "Concurrency.h"
#include "TestSupport.h"

#include "diag/LogRing.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace DiagTest
{

void runConcurrentWriters()
{
    std::cout << "\n-- concurrent producers --" << std::endl;

    constexpr int producerCount = 4;
    constexpr int recordsPerProducer = 500;
    constexpr int capacity = 256;

    ihub::diag::LogRing ring (capacity);

    std::atomic<bool> start { false };
    std::atomic<bool> running { true };
    std::atomic<int> torn { 0 };
    std::atomic<int> read { 0 };

    std::vector<std::thread> producers;

    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back ([&ring, &start, producer]
        {
            while (! start.load())
                std::this_thread::yield();

            for (int index = 0; index < recordsPerProducer; ++index)
                ring.log (ihub::diag::LogLevel::info,
                          ihub::diag::LogSource::general,
                          producer,
                          "producer " + std::to_string (producer)
                              + " record " + std::to_string (index));
        });
    }

    std::thread reader ([&ring, &running, &torn, &read]
    {
        ihub::diag::LogRecord buffer[capacity];

        while (running.load())
        {
            const auto count = ring.copyLatest (buffer, capacity);

            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& record = buffer[index];

                if (std::memchr (record.message, '\0',
                                 ihub::diag::logMessageCapacity) == nullptr)
                {
                    ++torn;
                    continue;
                }

                const auto expected = "producer " + std::to_string (record.code)
                                      + " record ";

                if (std::string (record.message).rfind (expected, 0) != 0)
                    ++torn;

                if (record.level != ihub::diag::LogLevel::info
                    || record.source != ihub::diag::LogSource::general)
                {
                    ++torn;
                }

                ++read;
            }

            std::this_thread::yield();
        }
    });

    start.store (true);

    for (auto& producer : producers)
        producer.join();

    running.store (false);
    reader.join();

    check (torn.load() == 0, "no reader observed a torn record");
    check (read.load() > 0, "the reader observed records while writers ran");
    check (ring.getPushedCount() == producerCount * recordsPerProducer,
           "all producers' records were accepted");

    ihub::diag::LogRecord newest[capacity];
    const auto count = ring.copyLatest (newest, capacity);
    bool contiguous = count == capacity;

    for (std::size_t index = 0; index < count && contiguous; ++index)
        contiguous = newest[index].sequence
                     == ring.getPushedCount() - count + index;

    check (contiguous, "the final window holds contiguous, newest records");
}

}
