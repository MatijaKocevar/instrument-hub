#include "Routes.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>

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
} // namespace detail

namespace
{
constexpr const char* jsonMimeType = "application/json; charset=utf-8";

juce::var makeObject()
{
    return juce::var (new juce::DynamicObject());
}

using detail::readBool;
using detail::readInteger;
using detail::readNumber;
} // namespace

//==============================================================================
std::string toJson (const juce::var& value)
{
    return juce::JSON::toString (value, false).toStdString();
}

void addCorsHeaders (httplib::Response& response)
{
    response.set_header ("Access-Control-Allow-Origin", "*");
    response.set_header ("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
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
    auto body = makeObject();
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

//==============================================================================
juce::var stateToVar (const StateSnapshot& state)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("schemaVersion", state.schemaVersion);

    auto application = makeObject();
    application.getDynamicObject()->setProperty ("name", juce::String (state.applicationName));
    application.getDynamicObject()->setProperty ("version",
                                                 juce::String (state.applicationVersion));
    object->setProperty ("application", application);

    object->setProperty ("armedSlot", state.armedSlot);
    object->setProperty ("uptimeMs", state.uptimeMs);

    juce::Array<juce::var> slots;

    for (const auto& slot : state.slots)
    {
        auto slotVar = makeObject();
        slotVar.getDynamicObject()->setProperty ("index", slot.index);
        slotVar.getDynamicObject()->setProperty ("name", juce::String (slot.name));
        slotVar.getDynamicObject()->setProperty ("armed", slot.armed);
        slots.add (slotVar);
    }

    object->setProperty ("slots", juce::var (slots));
    object->setProperty ("settings", settingsToVar (state.settings));
    object->setProperty ("transport", transportToVar (state.transport));

    return root;
}

juce::var settingsToVar (const SettingsState& settings)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("masterGain", settings.masterGain);
    object->setProperty ("inputGain", settings.inputGain);
    object->setProperty ("midiChannel", settings.midiChannel);
    object->setProperty ("metronome", settings.metronome);

    return root;
}

juce::var transportToVar (const TransportState& transport)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("playing", transport.playing);
    object->setProperty ("bpm", transport.bpm);
    object->setProperty ("ppq", transport.ppq);
    object->setProperty ("positionSamples", static_cast<juce::int64> (transport.positionSamples));

    return root;
}

juce::var meterFrameToVar (const MeterFrame& frame)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("type", "meters");
    object->setProperty ("sequence", static_cast<juce::int64> (frame.sequence));
    object->setProperty ("timestampMs", frame.timestampMs);

    auto master = makeObject();
    master.getDynamicObject()->setProperty ("peak", static_cast<double> (frame.masterPeak));
    object->setProperty ("master", master);

    juce::Array<juce::var> slots;

    for (const auto& slot : frame.slots)
    {
        auto slotVar = makeObject();
        slotVar.getDynamicObject()->setProperty ("slot", slot.slot);
        slotVar.getDynamicObject()->setProperty ("peak", static_cast<double> (slot.peak));
        slotVar.getDynamicObject()->setProperty ("gain", static_cast<double> (slot.gain));
        slotVar.getDynamicObject()->setProperty ("armed", slot.armed);
        slots.add (slotVar);
    }

    object->setProperty ("slots", juce::var (slots));

    return root;
}

//==============================================================================
void registerRoutes (httplib::Server& server, RouteContext context)
{
    server.Get ("/state", [context] (const httplib::Request&, httplib::Response& response)
    {
        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, stateToVar (context.state.snapshot()));
    });

    server.Post ("/arm", [context] (const httplib::Request& request,
                                   httplib::Response& response)
    {
        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();
        juce::var value = object->getProperty ("slot");

        if (value.isVoid())
            value = object->getProperty ("armedSlot");

        if (! object->hasProperty ("slot") && ! object->hasProperty ("armedSlot"))
        {
            setError (response, 400, "body must contain \"slot\" (a number, or null to disarm)");
            return;
        }

        int slot = -1;

        if (! value.isVoid())
        {
            std::int64_t parsed = 0;

            if (! readInteger (value, "slot",
                               -1,
                               std::numeric_limits<int>::max(),
                               parsed, error))
            {
                setError (response, 400, error);
                return;
            }

            slot = static_cast<int> (parsed);
        }

        if (! context.state.setArmedSlot (slot))
        {
            setError (response, 400, "slot is out of range");
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, stateToVar (context.state.snapshot()));
    });

    server.Get ("/settings", [context] (const httplib::Request&, httplib::Response& response)
    {
        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, settingsToVar (context.state.getSettings()));
    });

    server.Post ("/settings", [context] (const httplib::Request& request,
                                        httplib::Response& response)
    {
        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        if (! context.state.updateSettings (body, error))
        {
            setError (response, 400, error);
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, settingsToVar (context.state.getSettings()));
    });

    server.Get ("/transport", [context] (const httplib::Request&, httplib::Response& response)
    {
        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, transportToVar (context.state.getTransport()));
    });

    server.Post ("/transport", [context] (const httplib::Request& request,
                                         httplib::Response& response)
    {
        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        if (! context.state.updateTransport (body, error))
        {
            setError (response, 400, error);
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, transportToVar (context.state.getTransport()));
    });
}

} // namespace Api
