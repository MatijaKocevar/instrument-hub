#include "Routes.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>
#include <set>

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
        slotVar.getDynamicObject()->setProperty ("latencySamples", slot.latencySamples);
        slotVar.getDynamicObject()->setProperty ("latencyMs", slot.latencyMs);
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

namespace
{
juce::var channelArrayToVar (const std::vector<AudioChannelState>& channels)
{
    juce::Array<juce::var> result;

    for (const auto& channel : channels)
    {
        auto item = makeObject();
        item.getDynamicObject()->setProperty ("index", channel.index);
        item.getDynamicObject()->setProperty ("name", juce::String (channel.name));
        item.getDynamicObject()->setProperty ("active", channel.active);
        result.add (item);
    }

    return juce::var (result);
}

bool readString (const juce::var& value,
                 const char* name,
                 std::string& destination,
                 std::string& error)
{
    if (! value.isString())
    {
        error = std::string (name) + " must be a string";
        return false;
    }

    destination = value.toString().toStdString();
    return true;
}

bool readChannelList (const juce::var& value,
                      const char* name,
                      std::vector<int>& destination,
                      std::string& error)
{
    const auto* array = value.getArray();

    if (array == nullptr)
    {
        error = std::string (name) + " must be an array of channel indices";
        return false;
    }

    std::vector<int> channels;
    std::set<int> seen;

    for (const auto& item : *array)
    {
        std::int64_t parsed = 0;

        if (! readInteger (item, name, 0, 63, parsed, error))
            return false;

        if (! seen.insert (static_cast<int> (parsed)).second)
        {
            error = std::string (name) + " contains duplicate channel "
                    + juce::String (parsed).toStdString();
            return false;
        }

        channels.push_back (static_cast<int> (parsed));
    }

    destination = std::move (channels);
    return true;
}
} // namespace

juce::var audioStateToVar (const AudioStateSnapshot& audio)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("open", audio.open);
    object->setProperty ("deviceType", juce::String (audio.deviceType));
    object->setProperty ("deviceName", juce::String (audio.deviceName));

    juce::Array<juce::var> types;

    for (const auto& type : audio.deviceTypes)
        types.add (juce::String (type));

    object->setProperty ("deviceTypes", juce::var (types));

    juce::Array<juce::var> devices;

    for (const auto& device : audio.devices)
    {
        auto item = makeObject();
        item.getDynamicObject()->setProperty ("type", juce::String (device.type));
        item.getDynamicObject()->setProperty ("name", juce::String (device.name));
        devices.add (item);
    }

    object->setProperty ("devices", juce::var (devices));

    object->setProperty ("sampleRate", audio.sampleRate);
    object->setProperty ("bufferSize", audio.bufferSize);

    juce::Array<juce::var> rates;

    for (const auto rate : audio.sampleRates)
        rates.add (rate);

    object->setProperty ("sampleRates", juce::var (rates));

    juce::Array<juce::var> buffers;

    for (const auto size : audio.bufferSizes)
        buffers.add (size);

    object->setProperty ("bufferSizes", juce::var (buffers));

    object->setProperty ("inputChannels", channelArrayToVar (audio.inputChannels));
    object->setProperty ("outputChannels", channelArrayToVar (audio.outputChannels));

    auto latency = makeObject();
    latency.getDynamicObject()->setProperty ("bufferMs", audio.latency.bufferMs);
    latency.getDynamicObject()->setProperty ("deviceMs", audio.latency.deviceMs);
    latency.getDynamicObject()->setProperty ("chainMs", audio.latency.chainMs);
    latency.getDynamicObject()->setProperty ("totalMs", audio.latency.totalMs);
    object->setProperty ("latency", latency);

    object->setProperty ("xruns", static_cast<juce::int64> (audio.xruns));
    object->setProperty ("deviceXruns", static_cast<juce::int64> (audio.deviceXruns));
    object->setProperty ("callbackOverruns", static_cast<juce::int64> (audio.callbackOverruns));
    object->setProperty ("callbacks", static_cast<juce::int64> (audio.callbacks));
    object->setProperty ("loadPercent", audio.loadPercent);

    return root;
}

juce::var audioMeasurementToVar (const AudioMeasurementState& measurement)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("valid", measurement.valid);
    object->setProperty ("roundTripSamples", measurement.roundTripSamples);
    object->setProperty ("roundTripMs", measurement.roundTripMs);
    object->setProperty ("expectedMs", measurement.expectedMs);
    object->setProperty ("confidence", static_cast<double> (measurement.confidence));
    object->setProperty ("sampleRate", measurement.sampleRate);
    object->setProperty ("bufferSize", measurement.bufferSize);
    object->setProperty ("error", juce::String (measurement.error));

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

    // --- Audio device settings (T053) --------------------------------------

    server.Get ("/audio", [context] (const httplib::Request&, httplib::Response& response)
    {
        if (context.decorate != nullptr)
            context.decorate (response);

        const auto audio = context.audioProvider != nullptr
                               ? context.audioProvider()
                               : AudioStateSnapshot {};

        setJson (response, 200, audioStateToVar (audio));
    });

    server.Post ("/audio", [context] (const httplib::Request& request,
                                     httplib::Response& response)
    {
        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        AudioSettingsPatch patch;
        const auto& properties = body.getDynamicObject()->getProperties();

        if (properties.isEmpty())
        {
            setError (response, 400, "body must contain at least one audio setting");
            return;
        }

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);
            const auto key = name.toString();

            if (key == "deviceType")
            {
                std::string parsed;

                if (! readString (value, "deviceType", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.deviceType = std::move (parsed);
            }
            else if (key == "deviceName")
            {
                std::string parsed;

                if (! readString (value, "deviceName", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.deviceName = std::move (parsed);
            }
            else if (key == "sampleRate")
            {
                double parsed = 0.0;

                if (! readNumber (value, "sampleRate", 8000.0, 384000.0, parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.sampleRate = parsed;
            }
            else if (key == "bufferSize")
            {
                std::int64_t parsed = 0;

                if (! readInteger (value, "bufferSize", 16, 8192, parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.bufferSize = static_cast<int> (parsed);
            }
            else if (key == "inputChannels")
            {
                std::vector<int> parsed;

                if (! readChannelList (value, "inputChannels", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.inputChannels = std::move (parsed);
            }
            else if (key == "outputChannels")
            {
                std::vector<int> parsed;

                if (! readChannelList (value, "outputChannels", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.outputChannels = std::move (parsed);
            }
            else
            {
                setError (response, 400, "unknown audio setting: " + key.toStdString());
                return;
            }
        }

        if (context.audioApply == nullptr)
        {
            setError (response, 503, "no audio backend is attached");
            return;
        }

        if (! context.audioApply (patch, error))
        {
            setError (response, 400, error);
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        const auto audio = context.audioProvider != nullptr
                               ? context.audioProvider()
                               : AudioStateSnapshot {};

        setJson (response, 200, audioStateToVar (audio));
    });

    server.Post ("/audio/measure", [context] (const httplib::Request&,
                                             httplib::Response& response)
    {
        if (context.audioMeasure == nullptr)
        {
            setError (response, 503, "no audio backend is attached");
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, audioMeasurementToVar (context.audioMeasure()));
    });
}

} // namespace Api
