#include "LaunchkeyProtocol.h"

#include <algorithm>

namespace Launchkey
{

namespace
{
constexpr std::size_t headerSize   = sysExHeader.size();
constexpr std::size_t minimumFrame = headerSize + 2; // header, command, F7
} // namespace

std::uint8_t checksumOf (std::span<const std::uint8_t> commandAndPayload) noexcept
{
    std::uint8_t result = 0;

    for (const auto byte : commandAndPayload)
        result ^= static_cast<std::uint8_t> (byte & 0x7F);

    return static_cast<std::uint8_t> (result & 0x7F);
}

SysExFrame makeSysExFrame (SysExCommand command,
                           std::span<const std::uint8_t> payload,
                           Checksum checksum)
{
    SysExFrame frame;
    frame.checksum = checksum;

    for (const auto byte : payload)
        if (byte > 0x7F)
            return frame; // not representable in SysEx; leave the frame invalid

    const auto commandByte = static_cast<std::uint8_t> (command);

    frame.bytes.reserve (headerSize + payload.size()
                         + (checksum == Checksum::Xor7 ? 1u : 0u) + 2u);
    frame.bytes.insert (frame.bytes.end(), sysExHeader.begin(), sysExHeader.end());
    frame.bytes.push_back (commandByte);
    frame.bytes.insert (frame.bytes.end(), payload.begin(), payload.end());

    if (checksum == Checksum::Xor7)
    {
        std::uint8_t value = static_cast<std::uint8_t> (commandByte & 0x7F);

        for (const auto byte : payload)
            value ^= byte;

        frame.bytes.push_back (static_cast<std::uint8_t> (value & 0x7F));
    }

    frame.bytes.push_back (sysExEnd);

    return frame;
}

SysExFrame sessionModeFrame()
{
    constexpr std::array<std::uint8_t, 1> payload { 0x01 };
    return makeSysExFrame (SysExCommand::SessionMode, payload, Checksum::Xor7);
}

bool isSessionModeFrame (std::span<const std::uint8_t> bytes)
{
    const auto expected = sessionModeFrame();

    if (bytes.size() != expected.bytes.size())
        return false;

    return std::equal (bytes.begin(), bytes.end(), expected.bytes.begin());
}

bool SysExFrame::isValid() const noexcept
{
    if (bytes.size() < minimumFrame)
        return false;

    if (bytes.front() != sysExStart || bytes.back() != sysExEnd)
        return false;

    if (! std::equal (sysExHeader.begin(), sysExHeader.end(), bytes.begin()))
        return false;

    const bool hasChecksum = checksum == Checksum::Xor7;

    // Everything between the header and F7 (or the checksum) is 7-bit data.
    const auto dataEnd = hasChecksum ? bytes.size() - 2 : bytes.size() - 1;

    for (std::size_t i = headerSize; i < dataEnd; ++i)
        if (bytes[i] > 0x7F)
            return false;

    if (! hasChecksum)
        return true;

    const auto expected = checksumOf (
        { bytes.data() + headerSize, dataEnd - headerSize });

    return bytes[dataEnd] == expected;
}

} // namespace Launchkey
