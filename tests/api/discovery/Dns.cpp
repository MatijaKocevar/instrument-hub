#include "Dns.h"

namespace ApiTest::Discovery
{

#if ! defined (_WIN32)

std::uint16_t readU16 (const std::uint8_t* data)
{
    return static_cast<std::uint16_t> ((static_cast<std::uint16_t> (data[0]) << 8)
                                       | data[1]);
}

std::uint32_t readU32 (const std::uint8_t* data)
{
    return (static_cast<std::uint32_t> (data[0]) << 24)
           | (static_cast<std::uint32_t> (data[1]) << 16)
           | (static_cast<std::uint32_t> (data[2]) << 8)
           | static_cast<std::uint32_t> (data[3]);
}

bool readName (const std::uint8_t* data, std::size_t size, std::size_t& offset,
               Labels& labels)
{
    std::size_t position = offset;
    std::size_t resumeAt = 0;
    bool jumped = false;
    int jumps = 0;

    for (;;)
    {
        if (position >= size)
            return false;

        const auto length = data[position];

        if (length == 0)
        {
            ++position;
            break;
        }

        if ((length & 0xc0) == 0xc0)
        {
            if (position + 1 >= size)
                return false;

            const auto target = static_cast<std::size_t> (
                ((length & 0x3f) << 8) | data[position + 1]);

            if (! jumped)
            {
                resumeAt = position + 2;
                jumped = true;
            }

            if (++jumps > 16 || target >= size)
                return false;

            position = target;
            continue;
        }

        if (position + 1 + length > size)
            return false;

        labels.emplace_back (reinterpret_cast<const char*> (data + position + 1),
                             static_cast<std::size_t> (length));
        position += 1 + static_cast<std::size_t> (length);
    }

    offset = jumped ? resumeAt : position;
    return true;
}

std::string join (const Labels& labels)
{
    std::string result;

    for (const auto& label : labels)
    {
        if (! result.empty())
            result += '.';

        result += label;
    }

    return result;
}

bool parseMessage (const std::uint8_t* data, std::size_t size,
                   std::vector<Record>& records, bool& isResponse)
{
    if (size < 12)
        return false;

    const auto flags = readU16 (data + 2);
    isResponse = (flags & 0x8000) != 0;

    const auto questions = readU16 (data + 4);
    const auto answers = readU16 (data + 6);
    const auto authority = readU16 (data + 8);
    const auto additional = readU16 (data + 10);

    std::size_t offset = 12;

    for (std::uint16_t index = 0; index < questions; ++index)
    {
        Labels ignored;

        if (! readName (data, size, offset, ignored) || offset + 4 > size)
            return false;

        offset += 4;
    }

    const auto total = static_cast<std::uint32_t> (answers)
                       + authority + additional;

    for (std::uint32_t index = 0; index < total; ++index)
    {
        Record record;

        if (! readName (data, size, offset, record.name) || offset + 10 > size)
            return false;

        record.type = readU16 (data + offset);
        record.klass = readU16 (data + offset + 2);
        record.ttl = readU32 (data + offset + 4);
        const auto length = readU16 (data + offset + 8);
        offset += 10;

        if (offset + length > size)
            return false;

        record.rdata.assign (data + offset, data + offset + length);
        offset += length;
        records.push_back (std::move (record));
    }

    return true;
}

bool readRdataName (const Record& record, std::size_t start, Labels& labels)
{
    std::size_t offset = start;
    return readName (record.rdata.data(), record.rdata.size(), offset, labels);
}

std::vector<std::uint8_t> makePtrQuery (const Labels& name, std::uint16_t id)
{
    std::vector<std::uint8_t> packet;

    const auto appendU16 = [&packet] (std::uint16_t value)
    {
        packet.push_back (static_cast<std::uint8_t> (value >> 8));
        packet.push_back (static_cast<std::uint8_t> (value & 0xff));
    };

    appendU16 (id);
    appendU16 (0);
    appendU16 (1);
    appendU16 (0);
    appendU16 (0);
    appendU16 (0);

    for (const auto& label : name)
    {
        packet.push_back (static_cast<std::uint8_t> (label.size()));
        packet.insert (packet.end(), label.begin(), label.end());
    }

    packet.push_back (0);
    appendU16 (12);
    appendU16 (1);

    return packet;
}

#endif

}
