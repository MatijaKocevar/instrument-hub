#include "api/routes/RouteHelpers.h"

#include <cmath>
#include <cstdint>

namespace Api
{

namespace detail
{
bool isNumber (const juce::var& value) noexcept
{
    return value.isInt() || value.isInt64() || value.isDouble();
}

bool readNumber (const juce::var& value,
                 const char* name,
                 double minimum,
                 double maximum,
                 double& destination,
                 std::string& error)
{
    if (! isNumber (value))
    {
        error = std::string (name) + " must be a number";
        return false;
    }

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number) || number < minimum || number > maximum)
    {
        error = std::string (name) + " must be between "
                + juce::String (minimum).toStdString() + " and "
                + juce::String (maximum).toStdString();
        return false;
    }

    destination = number;
    return true;
}

bool readBool (const juce::var& value, const char* name, bool& destination, std::string& error)
{
    if (! value.isBool())
    {
        error = std::string (name) + " must be a boolean";
        return false;
    }

    destination = static_cast<bool> (value);
    return true;
}

bool readInteger (const juce::var& value,
                  const char* name,
                  std::int64_t minimum,
                  std::int64_t maximum,
                  std::int64_t& destination,
                  std::string& error)
{
    if (! isNumber (value))
    {
        error = std::string (name) + " must be an integer";
        return false;
    }

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number)
        || number < static_cast<double> (minimum)
        || number > static_cast<double> (maximum)
        || std::abs (number - std::floor (number)) > 1.0e-9)
    {
        error = std::string (name) + " must be an integer between "
                + juce::String (minimum).toStdString() + " and "
                + juce::String (maximum).toStdString();
        return false;
    }

    destination = static_cast<std::int64_t> (number);
    return true;
}
}

namespace
{
constexpr const char* jsonMimeType = "application/json; charset=utf-8";
}

std::string toJson (const juce::var& value)
{
    return juce::JSON::toString (value, false).toStdString();
}

void addCorsHeaders (httplib::Response& response)
{
    response.set_header ("Access-Control-Allow-Origin", "*");
    response.set_header ("Access-Control-Allow-Methods",
                         "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    response.set_header ("Access-Control-Allow-Headers",
                         "Content-Type, Authorization, X-Instrument-Hub-Token");
    response.set_header ("Access-Control-Max-Age", "600");
    response.set_header ("Cache-Control", "no-store");
}

void setJson (httplib::Response& response, int status, const juce::var& value)
{
    addCorsHeaders (response);
    response.status = status;
    response.set_content (toJson (value), jsonMimeType);
}

void setError (httplib::Response& response, int status, const std::string& message)
{
    auto body = Routes::makeObject();
    body.getDynamicObject()->setProperty ("error", juce::String (message));
    setJson (response, status, body);
}

bool parseJsonObject (const httplib::Request& request,
                      juce::var& result,
                      std::string& error)
{
    if (request.body.empty())
    {
        error = "request body must be a JSON object";
        return false;
    }

    auto parsed = juce::JSON::parse (juce::String (request.body));

    if (! parsed.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    result = parsed;
    return true;
}

}
