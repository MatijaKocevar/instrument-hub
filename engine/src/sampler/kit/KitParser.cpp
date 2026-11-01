#include "../KitLoader.h"
#include "KitJson.h"

#include <utility>

namespace ihub::sampler
{

namespace
{

using namespace kit_detail;

bool parseSample (const juce::var& value,
                  const juce::String& context,
                  KitSampleRef& destination,
                  juce::String& error)
{
    if (! value.isObject())
    {
        error = context + ": expected an object";
        return false;
    }

    const auto& object = *value.getDynamicObject();

    if (! hasOnlyProperties (object, { "path", "gain", "pan", "tune" }, context, error))
        return false;

    const juce::var* pathValue = nullptr;

    if (! getRequired (object, "path", context, pathValue, error))
        return false;

    juce::String path;

    if (! readString (*pathValue, context + ".path", false, path, error))
        return false;

    KitSampleRef result;
    result.path = path.toStdString();

    double gain = (double) result.gain;
    double pan = (double) result.pan;

    if (! readOptionalNumber (object, "gain", context, gain, error)
        || ! readOptionalNumber (object, "pan", context, pan, error)
        || ! readOptionalNumber (object, "tune", context, result.tuneSemitones, error))
    {
        return false;
    }

    result.gain = (float) gain;
    result.pan = (float) pan;

    destination = std::move (result);
    return true;
}

bool parseLayer (const juce::var& value,
                 const juce::String& context,
                 VelocityLayer& destination,
                 juce::String& error)
{
    if (! value.isObject())
    {
        error = context + ": expected an object";
        return false;
    }

    const auto& object = *value.getDynamicObject();

    if (! hasOnlyProperties (object, { "minVelocity", "maxVelocity", "roundRobin" },
                             context, error))
    {
        return false;
    }

    const juce::var* minValue = nullptr;
    const juce::var* maxValue = nullptr;
    const juce::var* roundRobinValue = nullptr;

    if (! getRequired (object, "minVelocity", context, minValue, error)
        || ! getRequired (object, "maxVelocity", context, maxValue, error)
        || ! getRequired (object, "roundRobin", context, roundRobinValue, error))
    {
        return false;
    }

    VelocityLayer result;

    if (! readInteger (*minValue, context + ".minVelocity", result.minVelocity, error)
        || ! readInteger (*maxValue, context + ".maxVelocity", result.maxVelocity, error))
    {
        return false;
    }

    if (! roundRobinValue->isArray())
    {
        error = context + ".roundRobin: expected an array";
        return false;
    }

    const auto* variants = roundRobinValue->getArray();

    for (int i = 0; i < variants->size(); ++i)
    {
        KitSampleRef sample;

        if (! parseSample (variants->getReference (i),
                           context + ".roundRobin[" + juce::String (i) + "]",
                           sample, error))
        {
            return false;
        }

        result.roundRobin.push_back (std::move (sample));
    }

    destination = std::move (result);
    return true;
}

bool parsePad (const juce::var& value,
               const juce::String& context,
               KitPad& destination,
               juce::String& error)
{
    if (! value.isObject())
    {
        error = context + ": expected an object";
        return false;
    }

    const auto& object = *value.getDynamicObject();

    if (! hasOnlyProperties (object, { "note", "name", "gain", "pan", "tune", "layers" },
                             context, error))
    {
        return false;
    }

    const juce::var* noteValue = nullptr;
    const juce::var* layersValue = nullptr;

    if (! getRequired (object, "note", context, noteValue, error)
        || ! getRequired (object, "layers", context, layersValue, error))
    {
        return false;
    }

    KitPad result;

    if (! readInteger (*noteValue, context + ".note", result.note, error))
        return false;

    if (! readOptionalString (object, "name", context, true, result.name, error))
        return false;

    double gain = (double) result.gain;
    double pan = (double) result.pan;

    if (! readOptionalNumber (object, "gain", context, gain, error)
        || ! readOptionalNumber (object, "pan", context, pan, error)
        || ! readOptionalNumber (object, "tune", context, result.tuneSemitones, error))
    {
        return false;
    }

    result.gain = (float) gain;
    result.pan = (float) pan;

    if (! layersValue->isArray())
    {
        error = context + ".layers: expected an array";
        return false;
    }

    const auto* layers = layersValue->getArray();

    for (int i = 0; i < layers->size(); ++i)
    {
        VelocityLayer layer;

        if (! parseLayer (layers->getReference (i),
                          context + ".layers[" + juce::String (i) + "]",
                          layer, error))
        {
            return false;
        }

        result.layers.push_back (std::move (layer));
    }

    destination = std::move (result);
    return true;
}

}

bool KitLoader::parse (const juce::String& json, Kit& destination, juce::String& error)
{
    error.clear();

    if (json.isEmpty())
    {
        error = "kit JSON is empty";
        return false;
    }

    if (json.getNumBytesAsUTF8() > (std::size_t) maximumKitBytes)
    {
        error = "Kit JSON is larger than the "
                + juce::String (maximumKitBytes) + " byte limit";
        return false;
    }

    const auto root = juce::JSON::parse (json);

    if (root.isVoid() || root.isUndefined())
    {
        error = "kit JSON is not valid JSON";
        return false;
    }

    if (! root.isObject())
    {
        error = "kit JSON is not an object";
        return false;
    }

    const auto& object = *root.getDynamicObject();

    if (! kit_detail::hasOnlyProperties (object, { "schemaVersion", "name", "pads" },
                                         "kit", error))
    {
        return false;
    }

    const juce::var* versionValue = nullptr;
    const juce::var* nameValue = nullptr;
    const juce::var* padsValue = nullptr;

    if (! kit_detail::getRequired (object, "schemaVersion", "kit", versionValue, error)
        || ! kit_detail::getRequired (object, "name", "kit", nameValue, error)
        || ! kit_detail::getRequired (object, "pads", "kit", padsValue, error))
    {
        return false;
    }

    Kit result;

    if (! kit_detail::readInteger (*versionValue, "kit.schemaVersion",
                                   result.schemaVersion, error))
        return false;

    juce::String name;

    if (! kit_detail::readString (*nameValue, "kit.name", false, name, error))
        return false;

    result.name = name.toStdString();

    if (! padsValue->isArray())
    {
        error = "kit.pads: expected an array";
        return false;
    }

    const auto* pads = padsValue->getArray();

    for (int i = 0; i < pads->size(); ++i)
    {
        KitPad pad;

        if (! parsePad (pads->getReference (i), "kit.pads[" + juce::String (i) + "]",
                        pad, error))
        {
            return false;
        }

        result.pads.push_back (std::move (pad));
    }

    if (! result.validate (error))
        return false;

    destination = std::move (result);
    return true;
}

}
