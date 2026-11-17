#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace Api {

struct NetworkMidiHandlers {
    std::function<void(const std::string& clientId, const std::string& name,
                       std::uint64_t session)>
        attach;

    std::function<void(const std::string& clientId, std::uint64_t session)> detach;

    std::function<bool(const std::string& clientId, std::uint64_t session,
                       const std::uint8_t* data, int size)>
        midi;
};

}  // namespace Api
