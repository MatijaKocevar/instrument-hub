#include "Wav.h"
#include "Constants.h"

#include <fstream>
#include <stdexcept>

namespace SelfTest
{

namespace
{

void putLe16 (std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back (static_cast<std::uint8_t> (value & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
}

void putLe32 (std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back (static_cast<std::uint8_t> (value & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 16) & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 24) & 0xff));
}

void appendFourCC (std::vector<std::uint8_t>& bytes, const char* text)
{
    bytes.insert (bytes.end(), text, text + 4);
}

} // namespace

void writeWav (const std::string& path, const std::vector<std::int16_t>& samples)
{
    const std::uint32_t dataBytes = static_cast<std::uint32_t> (samples.size() * sizeof (std::int16_t));
    std::vector<std::uint8_t> bytes;
    bytes.reserve (44 + dataBytes);

    appendFourCC (bytes, "RIFF");
    putLe32 (bytes, 36 + dataBytes);
    appendFourCC (bytes, "WAVE");
    appendFourCC (bytes, "fmt ");
    putLe32 (bytes, 16);
    putLe16 (bytes, 1);
    putLe16 (bytes, kChannels);
    putLe32 (bytes, kSampleRate);
    putLe32 (bytes, kSampleRate * kChannels * sizeof (std::int16_t));
    putLe16 (bytes, static_cast<std::uint16_t> (kChannels * sizeof (std::int16_t)));
    putLe16 (bytes, 16);
    appendFourCC (bytes, "data");
    putLe32 (bytes, dataBytes);

    for (const auto sample : samples)
    {
        const auto value = static_cast<std::uint16_t> (sample);
        bytes.push_back (static_cast<std::uint8_t> (value & 0xff));
        bytes.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
    }

    std::ofstream file (path, std::ios::binary | std::ios::trunc);

    if (! file)
        throw std::runtime_error ("cannot open " + path + " for writing");

    file.write (reinterpret_cast<const char*> (bytes.data()),
                static_cast<std::streamsize> (bytes.size()));

    if (! file)
        throw std::runtime_error ("failed while writing " + path);
}

} // namespace SelfTest
