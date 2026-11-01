#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ApiTest::Discovery
{

#if ! defined (_WIN32)

using Labels = std::vector<std::string>;

struct Record
{
    Labels name;
    std::uint16_t type = 0;
    std::uint16_t klass = 0;
    std::uint32_t ttl = 0;
    std::vector<std::uint8_t> rdata;
};

std::uint16_t readU16 (const std::uint8_t* data);

std::uint32_t readU32 (const std::uint8_t* data);

bool readName (const std::uint8_t* data, std::size_t size, std::size_t& offset,
               Labels& labels);

std::string join (const Labels& labels);

bool parseMessage (const std::uint8_t* data, std::size_t size,
                   std::vector<Record>& records, bool& isResponse);

bool readRdataName (const Record& record, std::size_t start, Labels& labels);

std::vector<std::uint8_t> makePtrQuery (const Labels& name, std::uint16_t id);

#endif

} // namespace ApiTest::Discovery
