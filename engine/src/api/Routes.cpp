#include "Routes.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstdlib>
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
        slotVar.getDynamicObject()->setProperty ("state", juce::String (slot.state));
        slotVar.getDynamicObject()->setProperty ("warmState", juce::String (slot.warmState));
        slotVar.getDynamicObject()->setProperty ("warm", slot.warm);
        slotVar.getDynamicObject()->setProperty ("family", juce::String (slot.family));
        slotVar.getDynamicObject()->setProperty ("familyName", juce::String (slot.familyName));
        slotVar.getDynamicObject()->setProperty ("patch", juce::String (slot.patch));
        slotVar.getDynamicObject()->setProperty ("patchName", juce::String (slot.patchName));
        slotVar.getDynamicObject()->setProperty ("gain", static_cast<double> (slot.gain));
        slotVar.getDynamicObject()->setProperty ("latencySamples", slot.latencySamples);
        slotVar.getDynamicObject()->setProperty ("latencyMs", slot.latencyMs);
        slots.add (slotVar);
    }

    object->setProperty ("slots", juce::var (slots));

    juce::Array<juce::var> families;

    for (const auto& family : state.families)
    {
        auto familyVar = makeObject();
        familyVar.getDynamicObject()->setProperty ("id", juce::String (family.id));
        familyVar.getDynamicObject()->setProperty ("name", juce::String (family.name));

        juce::Array<juce::var> patches;

        for (const auto& patch : family.patches)
        {
            auto patchVar = makeObject();
            patchVar.getDynamicObject()->setProperty ("slot", patch.slot);
            patchVar.getDynamicObject()->setProperty ("id", juce::String (patch.id));
            patchVar.getDynamicObject()->setProperty ("name", juce::String (patch.name));
            patchVar.getDynamicObject()->setProperty ("family", juce::String (patch.family));
            patchVar.getDynamicObject()->setProperty ("familyName",
                                                      juce::String (patch.familyName));

            auto instrument = makeObject();
            instrument.getDynamicObject()->setProperty (
                "id", juce::String (patch.instrument.id));
            instrument.getDynamicObject()->setProperty (
                "name", juce::String (patch.instrument.name));
            instrument.getDynamicObject()->setProperty (
                "kind", juce::String (patch.instrument.kind));
            instrument.getDynamicObject()->setProperty (
                "pluginFormat", juce::String (patch.instrument.pluginFormat));
            instrument.getDynamicObject()->setProperty (
                "pluginId", juce::String (patch.instrument.pluginId));
            instrument.getDynamicObject()->setProperty (
                "pluginName", juce::String (patch.instrument.pluginName));
            instrument.getDynamicObject()->setProperty (
                "pluginManufacturer", juce::String (patch.instrument.pluginManufacturer));
            instrument.getDynamicObject()->setProperty (
                "pluginVersion", juce::String (patch.instrument.pluginVersion));
            instrument.getDynamicObject()->setProperty (
                "samplePath", juce::String (patch.instrument.samplePath));
            patchVar.getDynamicObject()->setProperty ("instrument", instrument);

            patchVar.getDynamicObject()->setProperty ("volumeDb", patch.volumeDb);
            patchVar.getDynamicObject()->setProperty ("pan", patch.pan);
            patchVar.getDynamicObject()->setProperty ("mute", patch.mute);
            patchVar.getDynamicObject()->setProperty ("solo", patch.solo);
            patchVar.getDynamicObject()->setProperty ("colorIndex", patch.colorIndex);
            patchVar.getDynamicObject()->setProperty ("warmState",
                                                      juce::String (patch.warmState));
            patchVar.getDynamicObject()->setProperty ("warm", patch.warm);
            patchVar.getDynamicObject()->setProperty ("latencySamples",
                                                      patch.latencySamples);
            patchVar.getDynamicObject()->setProperty ("latencyMs", patch.latencyMs);
            patches.add (patchVar);
        }

        familyVar.getDynamicObject()->setProperty ("patches", juce::var (patches));
        families.add (familyVar);
    }

    object->setProperty ("families", juce::var (families));
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

juce::var fxChainToVar (const FxChainState& chain)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("slot", chain.slot);
    object->setProperty ("instrumentLatencySamples", chain.instrumentLatencySamples);
    object->setProperty ("fxLatencySamples", chain.fxLatencySamples);
    object->setProperty ("latencySamples", chain.latencySamples);
    object->setProperty ("latencyMs", chain.latencyMs);

    juce::Array<juce::var> inserts;

    for (const auto& insert : chain.inserts)
    {
        auto item = makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty ("index", insert.index);
        itemObject->setProperty ("type", juce::String (insert.type));
        itemObject->setProperty ("kind", juce::String (insert.kind));
        itemObject->setProperty ("id", juce::String (insert.id));
        itemObject->setProperty ("name", juce::String (insert.name));
        itemObject->setProperty ("bypassed", insert.bypassed);
        itemObject->setProperty ("wet", insert.wet);
        itemObject->setProperty ("dry", 1.0 - insert.wet);
        itemObject->setProperty ("latencySamples", insert.latencySamples);

        auto parameters = makeObject();

        for (const auto& [key, value] : insert.parameters)
            parameters.getDynamicObject()->setProperty (juce::String (key), value);

        itemObject->setProperty ("parameters", parameters);
        inserts.add (item);
    }

    object->setProperty ("inserts", juce::var (inserts));
    return root;
}

bool readFxParameters (const juce::var& value,
                       const char* name,
                       std::map<std::string, double>& destination,
                       std::string& error)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
    {
        error = std::string (name) + " must be an object of parameter values";
        return false;
    }

    std::map<std::string, double> parameters;
    const auto& properties = object->getProperties();

    for (int index = 0; index < properties.size(); ++index)
    {
        const auto& parameterValue = properties.getValueAt (index);

        if (! detail::isNumber (parameterValue))
        {
            error = std::string (name) + "." + properties.getName (index).toString().toStdString()
                    + " must be a number";
            return false;
        }

        const auto number = static_cast<double> (parameterValue);

        if (! std::isfinite (number))
        {
            error = std::string (name) + "." + properties.getName (index).toString().toStdString()
                    + " must be finite";
            return false;
        }

        parameters[properties.getName (index).toString().toStdString()] = number;
    }

    destination = std::move (parameters);
    return true;
}

bool readIntegerList (const juce::var& value,
                      const char* name,
                      std::vector<int>& destination,
                      std::string& error)
{
    const auto* array = value.getArray();

    if (array == nullptr)
    {
        error = std::string (name) + " must be an array of integers";
        return false;
    }

    std::vector<int> values;
    values.reserve (static_cast<std::size_t> (array->size()));

    for (const auto& item : *array)
    {
        std::int64_t parsed = 0;

        if (! readInteger (item, name, 0, 63, parsed, error))
            return false;

        values.push_back (static_cast<int> (parsed));
    }

    destination = std::move (values);
    return true;
}

bool parseFxInsert (const juce::var& value, FxInsertRequest& insert, std::string& error)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
    {
        error = "each insert must be an object";
        return false;
    }

    const auto& properties = object->getProperties();

    for (int index = 0; index < properties.size(); ++index)
    {
        const auto& name = properties.getName (index);
        const auto& item = properties.getValueAt (index);
        const auto key = name.toString();

        if (key == "type")
        {
            if (! readString (item, "type", insert.type, error))
                return false;
        }
        else if (key == "path")
        {
            if (! readString (item, "path", insert.path, error))
                return false;
        }
        else if (key == "id")
        {
            if (! readString (item, "id", insert.id, error))
                return false;
        }
        else if (key == "name")
        {
            if (! readString (item, "name", insert.name, error))
                return false;
        }
        else if (key == "bypassed")
        {
            if (! readBool (item, "bypassed", insert.bypassed, error))
                return false;
        }
        else if (key == "wet")
        {
            if (! readNumber (item, "wet", 0.0, 1.0, insert.wet, error))
                return false;
        }
        else if (key == "position")
        {
            std::int64_t parsed = 0;

            if (! readInteger (item, "position", -1, 1024, parsed, error))
                return false;

            insert.position = static_cast<int> (parsed);
        }
        else if (key == "parameters")
        {
            if (! readFxParameters (item, "parameters", insert.parameters, error))
                return false;
        }
        else
        {
            error = "unknown insert field: " + key.toStdString();
            return false;
        }
    }

    if (insert.type.empty())
    {
        error = "insert field \"type\" is required";
        return false;
    }

    return true;
}

bool parsePatchSource (const juce::var& value,
                       PatchSourceRequest& source,
                       std::string& error)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
    {
        error = "request body must be a JSON object";
        return false;
    }

    const auto& properties = object->getProperties();
    bool hasKind = false;

    for (int index = 0; index < properties.size(); ++index)
    {
        const auto& name = properties.getName (index);
        const auto& item = properties.getValueAt (index);
        const auto key = name.toString();

        if (key == "kind")
        {
            if (! readString (item, "kind", source.kind, error))
                return false;

            hasKind = true;
        }
        else if (key == "name")
        {
            if (! readString (item, "name", source.name, error))
                return false;
        }
        else if (key == "path")
        {
            if (! readString (item, "path", source.path, error))
                return false;
        }
        else if (key == "frequency")
        {
            if (! readNumber (item, "frequency", 20.0, 20000.0,
                              source.frequency, error))
                return false;
        }
        else if (key == "inputChannel")
        {
            std::int64_t parsed = 0;

            if (! readInteger (item, "inputChannel", 0, 63, parsed, error))
                return false;

            source.inputChannel = static_cast<int> (parsed);
        }
        else if (key == "pluginFormat")
        {
            if (! readString (item, "pluginFormat", source.pluginFormat, error))
                return false;
        }
        else if (key == "pluginId")
        {
            if (! readString (item, "pluginId", source.pluginId, error))
                return false;
        }
        else if (key == "pluginName")
        {
            if (! readString (item, "pluginName", source.pluginName, error))
                return false;
        }
        else if (key == "pluginManufacturer")
        {
            if (! readString (item, "pluginManufacturer",
                              source.pluginManufacturer, error))
                return false;
        }
        else if (key == "pluginVersion")
        {
            if (! readString (item, "pluginVersion", source.pluginVersion, error))
                return false;
        }
        else if (key == "samplePath")
        {
            if (! readString (item, "samplePath", source.samplePath, error))
                return false;
        }
        else
        {
            error = "unknown source field: " + key.toStdString();
            return false;
        }
    }

    if (! hasKind)
    {
        error = "source field \"kind\" is required";
        return false;
    }

    if (source.kind != "init-tone"
        && source.kind != "sampler"
        && source.kind != "plugin"
        && source.kind != "audio-input")
    {
        error = "kind must be init-tone, sampler, plugin or audio-input";
        return false;
    }

    if ((source.kind == "sampler" || source.kind == "plugin")
        && source.path.empty())
    {
        error = "source field \"path\" is required for " + source.kind;
        return false;
    }

    if (source.name.size() > 120)
    {
        error = "source field \"name\" is longer than 120 characters";
        return false;
    }

    return true;
}

bool parseFxUpdate (const juce::var& value, FxUpdateRequest& update, std::string& error)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
    {
        error = "request body must be a JSON object";
        return false;
    }

    const auto& properties = object->getProperties();
    bool any = false;

    for (int index = 0; index < properties.size(); ++index)
    {
        const auto& name = properties.getName (index);
        const auto& item = properties.getValueAt (index);
        const auto key = name.toString();

        if (key == "bypassed")
        {
            bool parsed = false;

            if (! readBool (item, "bypassed", parsed, error))
                return false;

            update.bypassed = parsed;
            any = true;
        }
        else if (key == "wet")
        {
            double parsed = 0.0;

            if (! readNumber (item, "wet", 0.0, 1.0, parsed, error))
                return false;

            update.wet = parsed;
            any = true;
        }
        else if (key == "position")
        {
            std::int64_t parsed = 0;

            if (! readInteger (item, "position", 0, 1024, parsed, error))
                return false;

            update.position = static_cast<int> (parsed);
            any = true;
        }
        else if (key == "parameters")
        {
            std::map<std::string, double> parsed;

            if (! readFxParameters (item, "parameters", parsed, error))
                return false;

            update.parameters = std::move (parsed);
            any = true;
        }
        else
        {
            error = "unknown insert update field: " + key.toStdString();
            return false;
        }
    }

    if (! any)
    {
        error = "body must contain at least one of bypassed, wet, position or parameters";
        return false;
    }

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

juce::var pairingToVar (const PairingSnapshot& pairing)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("lanEnabled", pairing.lanEnabled);
    object->setProperty ("address", juce::String (pairing.address));
    object->setProperty ("port", pairing.port);
    object->setProperty ("code", juce::String (pairing.code));
    object->setProperty ("codeExpiresInMs", pairing.codeExpiresInMs);
    object->setProperty ("payload", juce::String (pairing.payload));
    object->setProperty ("pairedClients", pairing.pairedClients);

    return root;
}

juce::var meterFrameToVar (const MeterFrame& frame){
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

juce::var pluginInventoryToVar (const PluginInventoryState& inventory)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    juce::Array<juce::var> plugins;

    for (const auto& plugin : inventory.plugins)
    {
        auto item = makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty ("id", juce::String (plugin.id));
        itemObject->setProperty ("name", juce::String (plugin.name));
        itemObject->setProperty ("description", juce::String (plugin.description));
        itemObject->setProperty ("vendor", juce::String (plugin.vendor));
        itemObject->setProperty ("version", juce::String (plugin.version));
        itemObject->setProperty ("category", juce::String (plugin.category));
        itemObject->setProperty ("isInstrument", plugin.isInstrument);
        itemObject->setProperty ("isEffect", plugin.isEffect);
        itemObject->setProperty ("format", juce::String (plugin.format));
        itemObject->setProperty ("path", juce::String (plugin.path));
        plugins.add (item);
    }

    object->setProperty ("plugins", juce::var (plugins));
    object->setProperty ("count", static_cast<int> (inventory.plugins.size()));
    object->setProperty ("total", inventory.total);
    object->setProperty ("search", juce::String (inventory.query));
    object->setProperty ("scanning", inventory.scanning);
    object->setProperty ("scanned", inventory.scanned);
    object->setProperty ("lastScanMs", static_cast<juce::int64> (inventory.lastScanMs));
    object->setProperty ("error", juce::String (inventory.error));

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

    // --- Patch mix / colour (T120) -----------------------------------------

    server.Patch (R"(/patches/(\d+))",
                  [context] (const httplib::Request& request,
                             httplib::Response& response)
    {
        int slot = -1;

        if (request.matches.size() > 1)
        {
            const auto text = request.matches[1].str();
            char* end = nullptr;
            const long parsed = std::strtol (text.c_str(), &end, 10);

            if (end != nullptr && *end == '\0'
                && parsed >= 0 && parsed < context.state.getNumSlots())
                slot = static_cast<int> (parsed);
        }

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        if (! context.state.updatePatch (slot, body, error))
        {
            setError (response, 400, error);
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, stateToVar (context.state.snapshot()));
    });

    // --- Instrument source (T124) ------------------------------------------

    server.Post (R"(/patches/(\d+)/source)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
    {
        int slot = -1;

        if (request.matches.size() > 1)
        {
            const auto text = request.matches[1].str();
            char* end = nullptr;
            const long parsed = std::strtol (text.c_str(), &end, 10);

            if (end != nullptr && *end == '\0'
                && parsed >= 0 && parsed < context.state.getNumSlots())
                slot = static_cast<int> (parsed);
        }

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        PatchSourceRequest source;

        if (! parsePatchSource (body, source, error))
        {
            setError (response, 400, error);
            return;
        }

        // A sampler or plug-in source must point at something that exists;
        // catching this here keeps a typo from silently silencing the slot.
        if ((source.kind == "sampler" || source.kind == "plugin")
            && ! juce::File (juce::String (source.path)).exists())
        {
            setError (response, 400, "source path does not exist: " + source.path);
            return;
        }

        if (! context.state.applyPatchSource (slot, source, error))
        {
            const int status = error.find ("no audio session") != std::string::npos
                ? 503
                : 400;
            setError (response, status, error);
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, stateToVar (context.state.snapshot()));
    });

    // --- Per-patch insert chain (T121) -------------------------------------

    const auto readFxSlot = [context] (const httplib::Request& request) -> int
    {
        if (request.matches.size() <= 1)
            return -1;

        const auto text = request.matches[1].str();
        char* end = nullptr;
        const long parsed = std::strtol (text.c_str(), &end, 10);

        if (end == nullptr || *end != '\0'
            || parsed < 0 || parsed >= context.state.getNumSlots())
            return -1;

        return static_cast<int> (parsed);
    };

    const auto readFxIndex = [] (const httplib::Request& request) -> int
    {
        if (request.matches.size() <= 2)
            return -1;

        const auto text = request.matches[2].str();
        char* end = nullptr;
        const long parsed = std::strtol (text.c_str(), &end, 10);

        if (end == nullptr || *end != '\0' || parsed < 0)
            return -1;

        return static_cast<int> (parsed);
    };

    const auto respondFx = [context] (httplib::Response& response,
                                      const FxChainState& chain)
    {
        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, fxChainToVar (chain));
    };

    const auto respondFxError = [] (httplib::Response& response, const std::string& error)
    {
        const int status = error.find ("no audio session") != std::string::npos ? 503 : 400;
        setError (response, status, error);
    };

    server.Get (R"(/patches/(\d+)/fx)",
                [context, readFxSlot, respondFx, respondFxError] (
                    const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readFxSlot (request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        FxChainState chain;
        std::string error;

        if (! context.state.getFxChain (slot, chain, error))
        {
            respondFxError (response, error);
            return;
        }

        respondFx (response, chain);
    });

    server.Post (R"(/patches/(\d+)/fx)",
                 [context, readFxSlot, respondFx, respondFxError] (
                     const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readFxSlot (request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        FxInsertRequest insert;

        if (! parseFxInsert (body, insert, error))
        {
            setError (response, 400, error);
            return;
        }

        FxCommand command;
        command.kind = FxCommandKind::add;
        command.inserts.push_back (std::move (insert));

        FxChainState chain;

        if (! context.state.applyFxCommand (slot, command, chain, error))
        {
            respondFxError (response, error);
            return;
        }

        respondFx (response, chain);
    });

    server.Put (R"(/patches/(\d+)/fx)",
                [context, readFxSlot, respondFx, respondFxError] (
                    const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readFxSlot (request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();
        juce::var chainValue = object->getProperty ("chain");

        if (chainValue.isVoid())
            chainValue = object->getProperty ("inserts");

        const auto* array = chainValue.getArray();

        if (array == nullptr)
        {
            setError (response, 400, "body must contain \"chain\" (an array)");
            return;
        }

        FxCommand command;
        command.kind = FxCommandKind::replace;

        for (const auto& item : *array)
        {
            FxInsertRequest insert;

            if (! parseFxInsert (item, insert, error))
            {
                setError (response, 400, error);
                return;
            }

            command.inserts.push_back (std::move (insert));
        }

        FxChainState chain;

        if (! context.state.applyFxCommand (slot, command, chain, error))
        {
            respondFxError (response, error);
            return;
        }

        respondFx (response, chain);
    });

    server.Patch (R"(/patches/(\d+)/fx/(\d+))",
                  [context, readFxSlot, readFxIndex, respondFx, respondFxError] (
                      const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readFxSlot (request);
        const int index = readFxIndex (request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        FxChainState current;
        std::string error;

        if (! context.state.getFxChain (slot, current, error))
        {
            respondFxError (response, error);
            return;
        }

        if (index < 0 || index >= static_cast<int> (current.inserts.size()))
        {
            setError (response, 404, "fx index is out of range");
            return;
        }

        juce::var body;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        FxUpdateRequest update;

        if (! parseFxUpdate (body, update, error))
        {
            setError (response, 400, error);
            return;
        }

        FxCommand command;
        command.kind = FxCommandKind::update;
        command.index = index;
        command.update = std::move (update);

        FxChainState chain;

        if (! context.state.applyFxCommand (slot, command, chain, error))
        {
            respondFxError (response, error);
            return;
        }

        respondFx (response, chain);
    });

    server.Delete (R"(/patches/(\d+)/fx/(\d+))",
                   [context, readFxSlot, readFxIndex, respondFx, respondFxError] (
                       const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readFxSlot (request);
        const int index = readFxIndex (request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        FxChainState current;
        std::string error;

        if (! context.state.getFxChain (slot, current, error))
        {
            respondFxError (response, error);
            return;
        }

        if (index < 0 || index >= static_cast<int> (current.inserts.size()))
        {
            setError (response, 404, "fx index is out of range");
            return;
        }

        FxCommand command;
        command.kind = FxCommandKind::remove;
        command.index = index;

        FxChainState chain;

        if (! context.state.applyFxCommand (slot, command, chain, error))
        {
            respondFxError (response, error);
            return;
        }

        respondFx (response, chain);
    });

    server.Post (R"(/patches/(\d+)/fx/reorder)",
                 [context, readFxSlot, respondFx, respondFxError] (
                     const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readFxSlot (request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();

        if (! object->hasProperty ("order"))
        {
            setError (response, 400, "body must contain \"order\" (an array of indices)");
            return;
        }

        FxCommand command;
        command.kind = FxCommandKind::reorder;

        if (! readIntegerList (object->getProperty ("order"), "order",
                               command.order, error))
        {
            setError (response, 400, error);
            return;
        }

        FxChainState chain;

        if (! context.state.applyFxCommand (slot, command, chain, error))
        {
            respondFxError (response, error);
            return;
        }

        respondFx (response, chain);
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

    // --- Plug-in inventory (T122) ------------------------------------------

    server.Get ("/plugins", [context] (const httplib::Request& request,
                                       httplib::Response& response)
    {
        std::string search;

        if (request.has_param ("search"))
            search = request.get_param_value ("search");
        else if (request.has_param ("q"))
            search = request.get_param_value ("q");

        PluginInventoryState inventory;

        try
        {
            if (context.pluginProvider != nullptr)
                inventory = context.pluginProvider (search);
        }
        catch (...)
        {
            setError (response, 500, "the plug-in inventory failed");
            return;
        }

        inventory.query = search;

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, pluginInventoryToVar (inventory));
    });

    server.Post ("/plugins/rescan", [context] (const httplib::Request&,
                                               httplib::Response& response)
    {
        if (context.pluginRescan == nullptr)
        {
            setError (response, 503, "plugin inventory is not available");
            return;
        }

        bool started = false;

        try
        {
            started = context.pluginRescan();
        }
        catch (...)
        {
            setError (response, 500, "could not start the plugin scan");
            return;
        }

        PluginInventoryState inventory;

        try
        {
            if (context.pluginProvider != nullptr)
                inventory = context.pluginProvider ({});
        }
        catch (...)
        {
            setError (response, 500, "the plug-in inventory failed");
            return;
        }

        auto body = pluginInventoryToVar (inventory);
        body.getDynamicObject()->setProperty ("started", started);

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, started ? 202 : 200, body);
    });

    // --- LAN pairing (T101) ------------------------------------------------

    server.Get ("/pairing", [context] (const httplib::Request&, httplib::Response& response)
    {
        if (context.pairing == nullptr)
        {
            setError (response, 503, "pairing is not available");
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, pairingToVar (context.pairing->snapshot()));
    });

    server.Post ("/pairing/lan", [context] (const httplib::Request& request,
                                            httplib::Response& response)
    {
        if (context.pairing == nullptr || context.setLanMode == nullptr)
        {
            setError (response, 503, "pairing is not available");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();

        if (! object->hasProperty ("enabled"))
        {
            setError (response, 400, "body must contain \"enabled\" (boolean)");
            return;
        }

        bool enabled = false;

        if (! readBool (object->getProperty ("enabled"), "enabled", enabled, error))
        {
            setError (response, 400, error);
            return;
        }

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, pairingToVar (context.setLanMode (enabled)));
    });

    server.Post ("/pairing/revoke", [context] (const httplib::Request&,
                                               httplib::Response& response)
    {
        if (context.pairing == nullptr)
        {
            setError (response, 503, "pairing is not available");
            return;
        }

        context.pairing->revokeAll();

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, pairingToVar (context.pairing->snapshot()));
    });

    server.Post ("/pair", [context] (const httplib::Request& request,
                                     httplib::Response& response)
    {
        if (context.pairing == nullptr)
        {
            setError (response, 503, "pairing is not available");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();
        std::string code;
        std::string clientName;

        if (! object->hasProperty ("code"))
        {
            setError (response, 400, "body must contain \"code\"");
            return;
        }

        if (! readString (object->getProperty ("code"), "code", code, error))
        {
            setError (response, 400, error);
            return;
        }

        if (object->hasProperty ("clientName"))
        {
            if (! readString (object->getProperty ("clientName"), "clientName",
                              clientName, error))
            {
                setError (response, 400, error);
                return;
            }
        }

        std::string token;

        if (! context.pairing->pair (code, token, error, std::move (clientName)))
        {
            setError (response, context.pairing->isLanEnabled() ? 401 : 403, error);
            return;
        }

        auto result = makeObject();
        result.getDynamicObject()->setProperty ("token", juce::String (token));
        result.getDynamicObject()->setProperty ("pairedClients",
                                                static_cast<int> (context.pairing->pairedClientCount()));

        if (context.decorate != nullptr)
            context.decorate (response);

        setJson (response, 200, result);
    });
}

} // namespace Api
