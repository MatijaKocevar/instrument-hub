#include "Serializer.h"

#include "Migrations.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace ihub::project
{

namespace
{

using juce::var;
using juce::DynamicObject;

var makeObject()
{
    return var (new DynamicObject());
}

juce::String fieldName (const juce::String& context, const char* key)
{
    const auto name = juce::String (key);
    return context.isEmpty() ? name : context + "." + name;
}

bool readString (const var& parent,
                 const char* key,
                 const juce::String& context,
                 std::string& destination,
                 juce::String& error,
                 bool required = true)
{
    auto* object = parent.getDynamicObject();

    if (object == nullptr)
    {
        error = context + ": expected an object";
        return false;
    }

    if (! object->hasProperty (key))
    {
        if (! required)
            return true;

        error = context + ": missing \"" + juce::String (key) + "\"";
        return false;
    }

    const auto value = object->getProperty (key);

    if (! value.isString())
    {
        error = fieldName (context, key) + ": expected a string";
        return false;
    }

    destination = value.toString().toStdString();
    return true;
}

bool readNumber (const var& parent,
                 const char* key,
                 const juce::String& context,
                 double& destination,
                 juce::String& error,
                 bool required = true)
{
    auto* object = parent.getDynamicObject();

    if (object == nullptr)
    {
        error = context + ": expected an object";
        return false;
    }

    if (! object->hasProperty (key))
    {
        if (! required)
            return true;

        error = context + ": missing \"" + juce::String (key) + "\"";
        return false;
    }

    const auto value = object->getProperty (key);

    if (! (value.isInt() || value.isInt64() || value.isDouble()))
    {
        error = fieldName (context, key) + ": expected a number";
        return false;
    }

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number))
    {
        error = fieldName (context, key) + ": must be finite";
        return false;
    }

    destination = number;
    return true;
}

bool readInteger (const var& parent,
                  const char* key,
                  const juce::String& context,
                  int& destination,
                  int minimum,
                  int maximum,
                  juce::String& error,
                  bool required = true)
{
    double number = static_cast<double> (destination);

    if (! readNumber (parent, key, context, number, error, required))
        return false;

    if (! parent.getDynamicObject()->hasProperty (key))
        return true;

    if (number < static_cast<double> (minimum) || number > static_cast<double> (maximum)
        || std::abs (number - std::floor (number)) > 1.0e-9)
    {
        error = fieldName (context, key) + ": expected an integer between "
                + juce::String (minimum) + " and " + juce::String (maximum);
        return false;
    }

    destination = static_cast<int> (number);
    return true;
}

bool readInteger64 (const var& parent,
                    const char* key,
                    const juce::String& context,
                    juce::int64& destination,
                    juce::String& error,
                    bool required = true)
{
    auto* object = parent.getDynamicObject();

    if (object == nullptr)
    {
        error = context + ": expected an object";
        return false;
    }

    if (! object->hasProperty (key))
    {
        if (! required)
            return true;

        error = context + ": missing \"" + juce::String (key) + "\"";
        return false;
    }

    const auto value = object->getProperty (key);

    if (! (value.isInt() || value.isInt64() || value.isDouble()))
    {
        error = fieldName (context, key) + ": expected an integer";
        return false;
    }

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number)
        || std::abs (number - std::floor (number)) > 1.0e-9
        || number < static_cast<double> (std::numeric_limits<juce::int64>::min())
        || number > static_cast<double> (std::numeric_limits<juce::int64>::max()))
    {
        error = fieldName (context, key) + ": expected an integer";
        return false;
    }

    destination = static_cast<juce::int64> (number);
    return true;
}

bool readBool (const var& parent,
               const char* key,
               const juce::String& context,
               bool& destination,
               juce::String& error,
               bool required = true)
{
    auto* object = parent.getDynamicObject();

    if (object == nullptr)
    {
        error = context + ": expected an object";
        return false;
    }

    if (! object->hasProperty (key))
    {
        if (! required)
            return true;

        error = context + ": missing \"" + juce::String (key) + "\"";
        return false;
    }

    const auto value = object->getProperty (key);

    if (! value.isBool())
    {
        error = fieldName (context, key) + ": expected a boolean";
        return false;
    }

    destination = static_cast<bool> (value);
    return true;
}

bool readObject (const var& parent,
                 const char* key,
                 const juce::String& context,
                 var& destination,
                 juce::String& error,
                 bool required = true)
{
    auto* object = parent.getDynamicObject();

    if (object == nullptr)
    {
        error = context + ": expected an object";
        return false;
    }

    if (! object->hasProperty (key))
    {
        if (! required)
            return true;

        error = context + ": missing \"" + juce::String (key) + "\"";
        return false;
    }

    destination = object->getProperty (key);

    if (! destination.isObject())
    {
        error = fieldName (context, key) + ": expected an object";
        return false;
    }

    return true;
}

bool readArray (const var& parent,
                const char* key,
                const juce::String& context,
                const juce::Array<var>*& destination,
                juce::String& error,
                bool required = true)
{
    auto* object = parent.getDynamicObject();

    if (object == nullptr)
    {
        error = context + ": expected an object";
        return false;
    }

    if (! object->hasProperty (key))
    {
        if (! required)
            return true;

        error = context + ": missing \"" + juce::String (key) + "\"";
        return false;
    }

    const auto value = object->getProperty (key);

    if (! value.isArray())
    {
        error = fieldName (context, key) + ": expected an array";
        return false;
    }

    destination = value.getArray();
    return destination != nullptr;
}

//==============================================================================
var instrumentToVar (const InstrumentConfig& instrument)
{
    auto value = makeObject();
    auto* object = value.getDynamicObject();

    object->setProperty ("kind", juce::String (toString (instrument.kind)));
    object->setProperty ("id", juce::String (instrument.id));
    object->setProperty ("name", juce::String (instrument.name));
    object->setProperty ("pluginState", juce::String (instrument.pluginState));
    object->setProperty ("inputChannel", instrument.inputChannel);
    object->setProperty ("gain", static_cast<double> (instrument.gain));
    object->setProperty ("directMonitor", instrument.directMonitor);

    return value;
}

var fxToVar (const FxConfig& fx)
{
    auto value = makeObject();
    auto* object = value.getDynamicObject();

    object->setProperty ("kind", juce::String (toString (fx.kind)));
    object->setProperty ("id", juce::String (fx.id));
    object->setProperty ("name", juce::String (fx.name));
    object->setProperty ("pluginState", juce::String (fx.pluginState));
    object->setProperty ("bypassed", fx.bypassed);

    auto parameters = makeObject();

    for (const auto& [key, parameterValue] : fx.parameters)
        parameters.getDynamicObject()->setProperty (juce::String (key),
                                                    static_cast<double> (parameterValue));

    object->setProperty ("parameters", parameters);
    return value;
}

var slotToVar (const SlotConfig& slot)
{
    auto value = makeObject();
    auto* object = value.getDynamicObject();

    object->setProperty ("index", slot.index);
    object->setProperty ("name", juce::String (slot.name));
    object->setProperty ("gain", static_cast<double> (slot.gain));
    object->setProperty ("colour", juce::String (slot.colour));
    object->setProperty ("active", slot.active);
    object->setProperty ("muted", slot.muted);
    object->setProperty ("instrument", instrumentToVar (slot.instrument));

    juce::Array<var> fxChain;

    for (const auto& fx : slot.fxChain)
        fxChain.add (fxToVar (fx));

    object->setProperty ("fx", juce::Array<var> (fxChain));
    return value;
}

var pluginStateToVar (const PluginStateChunk& chunk)
{
    auto value = makeObject();
    auto* object = value.getDynamicObject();

    object->setProperty ("uuid", juce::String (chunk.uuid));
    object->setProperty ("format", juce::String (chunk.format));
    object->setProperty ("pluginId", juce::String (chunk.pluginId));
    object->setProperty ("name", juce::String (chunk.name));
    object->setProperty ("manufacturer", juce::String (chunk.manufacturer));
    object->setProperty ("version", juce::String (chunk.version));
    object->setProperty ("bytes", static_cast<juce::int64> (chunk.data.getSize()));

    return value;
}

juce::String makeManifest (const SetFile& set)
{
    auto root = makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("schemaVersion", set.schemaVersion);
    object->setProperty ("name", juce::String (set.name));
    object->setProperty ("notes", juce::String (set.notes));

    auto transport = makeObject();
    transport.getDynamicObject()->setProperty ("bpm", set.transport.bpm);
    transport.getDynamicObject()->setProperty ("positionPpq", set.transport.positionPpq);
    transport.getDynamicObject()->setProperty ("playing", set.transport.playing);
    object->setProperty ("transport", transport);

    juce::Array<var> slots;

    for (const auto& slot : set.slots)
        slots.add (slotToVar (slot));

    object->setProperty ("slots", juce::Array<var> (slots));

    juce::Array<var> pluginStates;

    for (const auto& chunk : set.pluginStates)
        pluginStates.add (pluginStateToVar (chunk));

    object->setProperty ("pluginStates", juce::Array<var> (pluginStates));

    auto mapping = makeObject();
    mapping.getDynamicObject()->setProperty ("bytes",
                                             static_cast<juce::int64> (set.mappingYaml.size()));
    object->setProperty ("mapping", mapping);

    return juce::JSON::toString (root, false);
}

//==============================================================================

bool readEntry (juce::ZipFile& zip,
                const juce::String& name,
                juce::int64 maximumBytes,
                juce::MemoryBlock& destination,
                juce::String& error)
{
    const auto* entry = zip.getEntry (name);

    if (entry == nullptr)
    {
        error = "container is missing \"" + name + "\"";
        return false;
    }

    if (entry->uncompressedSize < 0 || entry->uncompressedSize > maximumBytes)
    {
        error = "container entry \"" + name + "\" is too large ("
                + juce::String (entry->uncompressedSize) + " bytes)";
        return false;
    }

    destination.setSize (static_cast<std::size_t> (entry->uncompressedSize));

    if (entry->uncompressedSize == 0)
        return true;

    std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));

    if (stream == nullptr)
    {
        error = "cannot open container entry \"" + name + "\"";
        return false;
    }

    const auto expectedBytes = static_cast<int> (entry->uncompressedSize);
    int totalBytesRead = 0;

    while (totalBytesRead < expectedBytes)
    {
        const auto bytesRead = stream->read (static_cast<char*> (destination.getData()) + totalBytesRead,
                                            expectedBytes - totalBytesRead);

        if (bytesRead <= 0)
            break;

        totalBytesRead += bytesRead;
    }

    if (totalBytesRead != expectedBytes)
    {
        error = "container entry \"" + name + "\" is truncated";
        return false;
    }

    return true;
}

bool addEntry (juce::ZipFile::Builder& builder,
               const juce::MemoryBlock& data,
               const juce::String& name,
               int compressionLevel)
{
    builder.addEntry (std::make_unique<juce::MemoryInputStream> (data, true),
                      compressionLevel,
                      name,
                      juce::Time::getCurrentTime());
    return true;
}

bool parseInstrument (const var& value,
                      const juce::String& context,
                      InstrumentConfig& instrument,
                      juce::String& error)
{
    std::string kind;

    if (! readString (value, "kind", context, kind, error))
        return false;

    if (! sourceKindFromString (kind, instrument.kind))
    {
        error = context + ".kind: unknown source kind \"" + juce::String (kind) + "\"";
        return false;
    }

    double gain = static_cast<double> (instrument.gain);
    int inputChannel = instrument.inputChannel;

    if (! readString (value, "id", context, instrument.id, error, false)
        || ! readString (value, "name", context, instrument.name, error, false)
        || ! readString (value, "pluginState", context, instrument.pluginState, error, false)
        || ! readInteger (value, "inputChannel", context, inputChannel, 0, 63, error, false)
        || ! readNumber (value, "gain", context, gain, error, false)
        || ! readBool (value, "directMonitor", context, instrument.directMonitor, error, false))
        return false;

    instrument.inputChannel = inputChannel;
    instrument.gain = static_cast<float> (gain);
    return true;
}

bool parseFx (const var& value,
              const juce::String& context,
              FxConfig& fx,
              juce::String& error)
{
    std::string kind;

    if (! readString (value, "kind", context, kind, error))
        return false;

    if (! fxKindFromString (kind, fx.kind))
    {
        error = context + ".kind: unknown fx kind \"" + juce::String (kind) + "\"";
        return false;
    }

    if (! readString (value, "id", context, fx.id, error)
        || ! readString (value, "name", context, fx.name, error, false)
        || ! readString (value, "pluginState", context, fx.pluginState, error, false)
        || ! readBool (value, "bypassed", context, fx.bypassed, error, false))
        return false;

    var parameters;

    if (! readObject (value, "parameters", context, parameters, error, false))
        return false;

    if (parameters.isObject())
    {
        const auto& properties = parameters.getDynamicObject()->getProperties();

        for (const auto& property : properties)
        {
            const auto key = property.name.toString();

            if (! (property.value.isInt() || property.value.isInt64() || property.value.isDouble()))
            {
                error = context + ".parameters." + key + ": expected a number";
                return false;
            }

            const auto number = static_cast<double> (property.value);

            if (! std::isfinite (number))
            {
                error = context + ".parameters." + key + ": must be finite";
                return false;
            }

            fx.parameters[key.toStdString()] = static_cast<float> (number);
        }
    }

    return true;
}

bool parseSlot (const var& value,
                const juce::String& context,
                SlotConfig& slot,
                juce::String& error)
{
    if (! value.isObject())
    {
        error = context + ": expected an object";
        return false;
    }

    double gain = static_cast<double> (slot.gain);

    if (! readInteger (value, "index", context, slot.index, 0, 63, error)
        || ! readString (value, "name", context, slot.name, error, false)
        || ! readNumber (value, "gain", context, gain, error, false)
        || ! readString (value, "colour", context, slot.colour, error, false)
        || ! readBool (value, "active", context, slot.active, error, false)
        || ! readBool (value, "muted", context, slot.muted, error, false))
        return false;

    slot.gain = static_cast<float> (gain);

    var instrument;

    if (! readObject (value, "instrument", context, instrument, error, false))
        return false;

    if (instrument.isObject() && ! parseInstrument (instrument, context + ".instrument",
                                                     slot.instrument, error))
        return false;

    const juce::Array<var>* fxChain = nullptr;

    if (! readArray (value, "fx", context, fxChain, error, false))
        return false;

    if (fxChain != nullptr)
    {
        for (int i = 0; i < fxChain->size(); ++i)
        {
            const auto& fxValue = fxChain->getReference (i);
            const auto fxContext = context + ".fx[" + juce::String (i) + "]";

            if (! fxValue.isObject())
            {
                error = fxContext + ": expected an object";
                return false;
            }

            FxConfig fx;

            if (! parseFx (fxValue, fxContext, fx, error))
                return false;

            slot.fxChain.push_back (std::move (fx));
        }
    }

    return true;
}

bool parseTransport (const var& value,
                     const juce::String& context,
                     TransportConfig& transport,
                     juce::String& error)
{
    if (! value.isObject())
    {
        error = context + ": expected an object";
        return false;
    }

    double bpm = transport.bpm;
    double positionPpq = transport.positionPpq;

    if (! readNumber (value, "bpm", context, bpm, error, false)
        || ! readNumber (value, "positionPpq", context, positionPpq, error, false)
        || ! readBool (value, "playing", context, transport.playing, error, false))
        return false;

    transport.bpm = bpm;
    transport.positionPpq = positionPpq;
    return true;
}

bool parseManifest (const juce::MemoryBlock& manifest,
                    SetFile& set,
                    juce::int64& declaredMappingBytes,
                    juce::String& error)
{
    if (manifest.getSize() == 0)
    {
        error = "set.json is empty";
        return false;
    }

    const auto text = juce::String::fromUTF8 (static_cast<const char*> (manifest.getData()),
                                              static_cast<int> (manifest.getSize()));
    auto root = juce::JSON::parse (text);

    if (! root.isObject())
    {
        error = "set.json is not a JSON object";
        return false;
    }

    if (! migrateManifest (root, error))
        return false;

    set.schemaVersion = currentSetSchemaVersion;

    if (! readString (root, "name", "set", set.name, error))
        return false;

    if (! readString (root, "notes", "set", set.notes, error, false))
        return false;

    var transport;

    if (! readObject (root, "transport", "set", transport, error, false))
        return false;

    if (transport.isObject()
        && ! parseTransport (transport, "set.transport", set.transport, error))
        return false;

    var mapping;

    if (! readObject (root, "mapping", "set", mapping, error, false))
        return false;

    if (mapping.isObject()
        && ! readInteger64 (mapping, "bytes", "set.mapping", declaredMappingBytes, error, false))
        return false;

    const juce::Array<var>* slots = nullptr;

    if (! readArray (root, "slots", "set", slots, error))
        return false;

    for (int i = 0; i < slots->size(); ++i)
    {
        const auto& value = slots->getReference (i);
        const auto context = "set.slots[" + juce::String (i) + "]";

        if (! value.isObject())
        {
            error = context + ": expected an object";
            return false;
        }

        SlotConfig slot;

        if (! parseSlot (value, context, slot, error))
            return false;

        set.slots.push_back (std::move (slot));
    }

    return true;
}

bool parsePluginStates (const juce::MemoryBlock& manifest,
                        juce::ZipFile& zip,
                        SetFile& set,
                        juce::String& error)
{
    const auto text = juce::String::fromUTF8 (static_cast<const char*> (manifest.getData()),
                                              static_cast<int> (manifest.getSize()));
    const auto root = juce::JSON::parse (text);
    const juce::Array<var>* states = nullptr;

    if (! readArray (root, "pluginStates", "set", states, error))
        return false;

    for (int i = 0; i < states->size(); ++i)
    {
        const auto& value = states->getReference (i);
        const auto context = "set.pluginStates[" + juce::String (i) + "]";

        if (! value.isObject())
        {
            error = context + ": expected an object";
            return false;
        }

        PluginStateChunk chunk;
        juce::int64 declaredBytes = -1;

        if (! readString (value, "uuid", context, chunk.uuid, error)
            || ! readString (value, "format", context, chunk.format, error, false)
            || ! readString (value, "pluginId", context, chunk.pluginId, error, false)
            || ! readString (value, "name", context, chunk.name, error, false)
            || ! readString (value, "manufacturer", context, chunk.manufacturer, error, false)
            || ! readString (value, "version", context, chunk.version, error, false)
            || ! readInteger64 (value, "bytes", context, declaredBytes, error))
            return false;

        if (! isValidPluginStateId (chunk.uuid))
        {
            error = context + ".uuid is not a safe entry name: \""
                    + juce::String (chunk.uuid) + "\"";
            return false;
        }

        if (declaredBytes < 0 || declaredBytes > Serializer::maximumPluginStateBytes)
        {
            error = context + ".bytes is out of range";
            return false;
        }

        if (! readEntry (zip, juce::String (pluginStateEntryPath (chunk.uuid)),
                         Serializer::maximumPluginStateBytes, chunk.data, error))
            return false;

        if (declaredBytes != static_cast<juce::int64> (chunk.data.getSize()))
        {
            error = context + ".bytes does not match the stored chunk ("
                    + juce::String (declaredBytes) + " declared, "
                    + juce::String (static_cast<juce::int64> (chunk.data.getSize()))
                    + " stored)";
            return false;
        }

        set.pluginStates.push_back (std::move (chunk));
    }

    return true;
}

} // namespace

bool Serializer::save (const SetFile& set, const juce::File& file, juce::String& error)
{
    if (! set.validate (error))
        return false;

    const auto manifest = makeManifest (set);

    juce::ZipFile::Builder builder;

    const juce::MemoryBlock manifestData (manifest.toRawUTF8(),
                                          static_cast<std::size_t> (manifest.getNumBytesAsUTF8()));
    addEntry (builder, manifestData, manifestEntry, 6);

    const juce::MemoryBlock mappingData (set.mappingYaml.data(), set.mappingYaml.size());
    addEntry (builder, mappingData, mappingEntry, 6);

    for (const auto& chunk : set.pluginStates)
    {
        addEntry (builder,
                  chunk.data,
                  juce::String (pluginStateEntryPath (chunk.uuid)),
                  6);
    }

    const auto parent = file.getParentDirectory();

    if (! parent.exists() && ! parent.createDirectory())
    {
        error = "cannot create directory " + parent.getFullPathName();
        return false;
    }

    const auto temporary = parent.getChildFile (file.getFileName() + ".tmp");
    temporary.deleteFile();

    {
        juce::FileOutputStream output (temporary);

        if (! output.openedOk())
        {
            error = "cannot open " + temporary.getFullPathName() + " for writing";
            temporary.deleteFile();
            return false;
        }

        if (! builder.writeToStream (output, nullptr))
        {
            error = "could not write the .ihub archive";
            temporary.deleteFile();
            return false;
        }

        output.flush();

        if (! output.getStatus().wasOk())
        {
            error = "could not flush the .ihub archive: "
                    + output.getStatus().getErrorMessage();
            temporary.deleteFile();
            return false;
        }
    }

    if (! temporary.replaceFileIn (file))
    {
        error = "could not replace " + file.getFullPathName();
        temporary.deleteFile();
        return false;
    }

    error.clear();
    return true;
}

bool Serializer::load (const juce::File& file, SetFile& destination, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "set file does not exist: " + file.getFullPathName();
        return false;
    }

    juce::ZipFile zip (file);

    if (zip.getNumEntries() <= 0)
    {
        error = "not a readable .ihub container: " + file.getFullPathName();
        return false;
    }

    juce::int64 totalUncompressed = 0;

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry (i);

        if (entry == nullptr)
        {
            error = "corrupt .ihub container: missing entry metadata";
            return false;
        }

        if (entry->uncompressedSize < 0)
        {
            error = "corrupt .ihub container: negative entry size";
            return false;
        }

        totalUncompressed += entry->uncompressedSize;

        if (totalUncompressed > maximumTotalUncompressedBytes)
        {
            error = "container expands to more than the supported maximum";
            return false;
        }
    }

    juce::MemoryBlock manifest;

    if (! readEntry (zip, manifestEntry, maximumManifestBytes, manifest, error))
        return false;

    SetFile set;
    juce::int64 declaredMappingBytes = -1;

    if (! parseManifest (manifest, set, declaredMappingBytes, error))
        return false;

    if (! parsePluginStates (manifest, zip, set, error))
        return false;

    juce::MemoryBlock mappingData;

    if (! readEntry (zip, mappingEntry, maximumMappingBytes, mappingData, error))
        return false;

    if (declaredMappingBytes >= 0
        && declaredMappingBytes != static_cast<juce::int64> (mappingData.getSize()))
    {
        error = "set.mapping.bytes does not match mapping.yaml ("
                + juce::String (declaredMappingBytes) + " declared, "
                + juce::String (static_cast<juce::int64> (mappingData.getSize()))
                + " stored)";
        return false;
    }

    if (mappingData.getSize() > 0)
        set.mappingYaml.assign (static_cast<const char*> (mappingData.getData()),
                                mappingData.getSize());

    if (! set.validate (error))
        return false;

    destination = std::move (set);
    error.clear();
    return true;
}

} // namespace ihub::project
