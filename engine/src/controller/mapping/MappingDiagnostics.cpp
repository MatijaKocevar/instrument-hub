#include "controller/mapping/MappingDiagnostics.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

namespace Controller::MappingDetail
{

bool fail (MappingError& error, const YamlValue& value, const std::string& message)
{
    error = { value.line, message };
    return false;
}

bool checkKeys (const YamlValue& value,
                std::initializer_list<std::string_view> allowed,
                MappingError& error)
{
    for (const auto& [key, child] : value.mapping)
    {
        (void) child;

        bool known = false;

        for (const auto entry : allowed)
            if (key == entry)
                known = true;

        if (! known)
            return fail (error, value, "unknown key '" + key + "'");
    }

    return true;
}

bool readString (const YamlValue& value, std::string& out, MappingError& error,
                 const std::string& what, bool allowEmpty)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be a string");

    if (! allowEmpty && value.scalar.empty())
        return fail (error, value, what + " must not be empty");

    out = value.scalar;
    return true;
}

bool readInt (const YamlValue& value, int& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be an integer");

    std::string text = value.scalar;

    if (! text.empty() && text.front() == '+')
        text.erase (text.begin());

    if (text.empty())
        return fail (error, value, what + " must be an integer");

    int parsed = 0;
    const auto result = std::from_chars (text.data(), text.data() + text.size(), parsed);

    if (result.ec != std::errc() || result.ptr != text.data() + text.size())
        return fail (error, value, what + " must be an integer");

    out = parsed;
    return true;
}

bool readFloat (const YamlValue& value, float& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be a number");

    if (value.scalar.empty())
        return fail (error, value, what + " must be a number");

    char* end = nullptr;
    const float parsed = std::strtof (value.scalar.c_str(), &end);

    if (end == value.scalar.c_str() || *end != '\0' || ! std::isfinite (parsed))
        return fail (error, value, what + " must be a finite number");

    out = parsed;
    return true;
}

bool readBool (const YamlValue& value, bool& out, MappingError& error, const std::string& what)
{
    if (value.kind != YamlKind::scalar)
        return fail (error, value, what + " must be true or false");

    if (value.scalar == "true")
    {
        out = true;
        return true;
    }

    if (value.scalar == "false")
    {
        out = false;
        return true;
    }

    return fail (error, value, what + " must be true or false");
}

bool readColourIndex (const YamlValue& value, int& out, MappingError& error,
                      const std::string& what)
{
    if (! readInt (value, out, error, what))
        return false;

    if (out < 0 || out > SetMapping::maximumColourIndex)
        return fail (error, value, what + " must be within 0.."
                         + std::to_string (SetMapping::maximumColourIndex));

    return true;
}

}
