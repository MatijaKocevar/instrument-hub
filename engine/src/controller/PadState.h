#pragma once

#include <cstdint>

namespace Controller {

enum class PadState : std::uint8_t { idle = 0, warming, active, error };

inline const char* toString(PadState state) noexcept {
    switch (state) {
        case PadState::idle:
            return "idle";
        case PadState::warming:
            return "warming";
        case PadState::active:
            return "active";
        case PadState::error:
            return "error";
    }

    return "idle";
}

}  // namespace Controller
