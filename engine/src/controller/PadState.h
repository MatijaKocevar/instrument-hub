#pragma once

#include <cstdint>

namespace Controller
{

/** One pad's feedback state, shared by every supported controller. */
enum class PadState : std::uint8_t
{
    idle = 0,
    warming,
    active,
    error
};

inline const char* toString (PadState state) noexcept
{
    switch (state)
    {
        case PadState::idle:    return "idle";
        case PadState::warming: return "warming";
        case PadState::active:  return "active";
        case PadState::error:   return "error";
    }

    return "idle";
}

} // namespace Controller
