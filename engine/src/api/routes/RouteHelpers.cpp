#include "api/routes/RouteHelpers.h"

#include <cstdlib>

namespace Api::Routes {

juce::var makeObject() { return juce::var(new juce::DynamicObject()); }

bool readString(const juce::var& value, const char* name, std::string& destination,
                std::string& error) {
    if (!value.isString()) {
        error = std::string(name) + " must be a string";
        return false;
    }

    destination = value.toString().toStdString();
    return true;
}

bool readIntegerList(const juce::var& value, const char* name, std::vector<int>& destination,
                     std::string& error) {
    const auto* array = value.getArray();

    if (array == nullptr) {
        error = std::string(name) + " must be an array of integers";
        return false;
    }

    std::vector<int> values;
    values.reserve(static_cast<std::size_t>(array->size()));

    for (const auto& item : *array) {
        std::int64_t parsed = 0;

        if (!detail::readInteger(item, name, 0, 63, parsed, error)) return false;

        values.push_back(static_cast<int>(parsed));
    }

    destination = std::move(values);
    return true;
}

int readPatchSlot(const RouteContext& context, const httplib::Request& request) {
    if (request.matches.size() <= 1) return -1;

    const auto text = request.matches[1].str();
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);

    if (end == nullptr || *end != '\0' || parsed < 0 || parsed >= context.state.getNumSlots())
        return -1;

    return static_cast<int>(parsed);
}

int readFxIndex(const httplib::Request& request) {
    if (request.matches.size() <= 2) return -1;

    const auto text = request.matches[2].str();
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);

    if (end == nullptr || *end != '\0' || parsed < 0) return -1;

    return static_cast<int>(parsed);
}

void decorate(const RouteContext& context, httplib::Response& response) {
    if (context.decorate != nullptr) context.decorate(response);
}

}  // namespace Api::Routes
