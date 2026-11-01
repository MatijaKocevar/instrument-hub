#include "project/serializer/PluginStateBlob.h"

#include "project/Serializer.h"
#include "project/serializer/Container.h"
#include "project/serializer/JsonFields.h"

#include <utility>

namespace ihub::project::serializer
{

using juce::var;

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

} // namespace ihub::project::serializer
