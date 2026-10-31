#include "KitLoader.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <map>
#include <utility>

namespace ihub::sampler
{

float panGainLeft (float pan, float gain) noexcept
{
    const auto clamped = juce::jlimit (-1.0f, 1.0f, pan);
    const auto angle = (clamped + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    return std::cos (angle) * gain;
}

float panGainRight (float pan, float gain) noexcept
{
    const auto clamped = juce::jlimit (-1.0f, 1.0f, pan);
    const auto angle = (clamped + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    return std::sin (angle) * gain;
}

//==============================================================================
// LoadedKit

int LoadedKit::findPadBinding (int note) const noexcept
{
    for (std::size_t i = 0; i < padBindings.size(); ++i)
        if (padBindings[i].note == note)
            return (int) i;

    return -1;
}

ResolvedHit LoadedKit::makeHit (std::size_t layerBindingIndex,
                                int variant,
                                int note,
                                int velocity) const noexcept
{
    if (layerBindingIndex >= layerBindings.size())
        return {};

    const auto& binding = layerBindings[layerBindingIndex];

    if (binding.padIndex >= kit.pads.size() || binding.sampleCount == 0)
        return {};

    const auto& pad = kit.pads[binding.padIndex];

    if (binding.layerIndex >= pad.layers.size())
        return {};

    const auto& layer = pad.layers[binding.layerIndex];
    const auto variantIndex = (std::size_t) (variant < 0 ? 0 : variant) % binding.sampleCount;

    if (binding.firstSample + variantIndex >= samplePointers.size())
        return {};

    const auto* sample = samplePointers[binding.firstSample + variantIndex];
    const auto& sampleRef = layer.roundRobin[variantIndex];

    ResolvedHit hit;
    hit.sample = sample;
    hit.pad = &pad;
    hit.layer = &layer;
    hit.note = note;
    hit.velocity = velocity;
    hit.variant = (int) variantIndex;
    hit.gain = pad.gain * sampleRef.gain;
    hit.pan = juce::jlimit (-1.0f, 1.0f, pad.pan + sampleRef.pan);
    hit.playbackRatio = pad.playbackRatio() * sampleRef.playbackRatio();

    return hit;
}

ResolvedHit LoadedKit::resolve (int note, int velocity) noexcept
{
    const auto padIndex = findPadBinding (note);

    if (padIndex < 0)
        return {};

    const auto& padBinding = padBindings[(std::size_t) padIndex];

    for (std::size_t i = 0; i < padBinding.numLayers; ++i)
    {
        const auto layerIndex = padBinding.firstLayer + i;

        if (layerIndex >= layerBindings.size())
            break;

        const auto& binding = layerBindings[layerIndex];
        const auto& layer = kit.pads[binding.padIndex].layers[binding.layerIndex];

        if (! layer.contains (velocity))
            continue;

        if (roundRobinCounters == nullptr || layerIndex >= numRoundRobinCounters)
            return {};

        const auto count = roundRobinCounters[layerIndex].fetch_add (1, std::memory_order_relaxed);
        return makeHit (layerIndex, (int) (count % (std::uint32_t) binding.sampleCount),
                        note, velocity);
    }

    return {};
}

ResolvedHit LoadedKit::resolveWithVariant (int note, int velocity, int variantIndex) const noexcept
{
    const auto padIndex = findPadBinding (note);

    if (padIndex < 0)
        return {};

    const auto& padBinding = padBindings[(std::size_t) padIndex];

    for (std::size_t i = 0; i < padBinding.numLayers; ++i)
    {
        const auto layerIndex = padBinding.firstLayer + i;

        if (layerIndex >= layerBindings.size())
            break;

        const auto& binding = layerBindings[layerIndex];
        const auto& layer = kit.pads[binding.padIndex].layers[binding.layerIndex];

        if (layer.contains (velocity))
            return makeHit (layerIndex, variantIndex, note, velocity);
    }

    return {};
}

//==============================================================================
// KitLoader

namespace
{

using DynamicObject = juce::DynamicObject;

const juce::var* findProperty (const DynamicObject& object, const char* name) noexcept
{
    return object.getProperties().getVarPointer (name);
}

bool hasOnlyProperties (const DynamicObject& object,
                        std::initializer_list<const char*> allowed,
                        const juce::String& context,
                        juce::String& error)
{
    for (const auto& property : object.getProperties())
    {
        bool known = false;

        for (const auto* name : allowed)
        {
            if (property.name.toString() == name)
            {
                known = true;
                break;
            }
        }

        if (! known)
        {
            error = context + ": unknown property \"" + property.name.toString() + "\"";
            return false;
        }
    }

    return true;
}

bool getRequired (const DynamicObject& object,
                  const char* name,
                  const juce::String& context,
                  const juce::var*& value,
                  juce::String& error)
{
    value = findProperty (object, name);

    if (value == nullptr)
    {
        error = context + ": missing \"" + name + "\"";
        return false;
    }

    return true;
}

bool readString (const juce::var& value,
                 const juce::String& context,
                 bool allowEmpty,
                 juce::String& destination,
                 juce::String& error)
{
    if (! value.isString())
    {
        error = context + ": expected a string";
        return false;
    }

    const auto text = value.toString();

    if (! allowEmpty && text.isEmpty())
    {
        error = context + ": must not be empty";
        return false;
    }

    destination = text;
    return true;
}

bool readNumber (const juce::var& value,
                 const juce::String& context,
                 double& destination,
                 juce::String& error)
{
    if (! (value.isInt() || value.isInt64() || value.isDouble()))
    {
        error = context + ": expected a number";
        return false;
    }

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number))
    {
        error = context + ": must be finite";
        return false;
    }

    destination = number;
    return true;
}

bool readInteger (const juce::var& value,
                  const juce::String& context,
                  int& destination,
                  juce::String& error)
{
    double number = 0.0;

    if (! readNumber (value, context, number, error))
        return false;

    if (number < (double) std::numeric_limits<int>::min()
        || number > (double) std::numeric_limits<int>::max()
        || std::abs (std::floor (number) - number) > 1.0e-9)
    {
        error = context + ": expected an integer";
        return false;
    }

    destination = (int) number;
    return true;
}

bool readOptionalNumber (const DynamicObject& object,
                         const char* name,
                         const juce::String& context,
                         double& destination,
                         juce::String& error)
{
    const auto* value = findProperty (object, name);

    if (value == nullptr)
        return true;

    return readNumber (*value, context + "." + name, destination, error);
}

bool readOptionalString (const DynamicObject& object,
                         const char* name,
                         const juce::String& context,
                         bool allowEmpty,
                         std::string& destination,
                         juce::String& error)
{
    const auto* value = findProperty (object, name);

    if (value == nullptr)
        return true;

    juce::String text;

    if (! readString (*value, context + "." + name, allowEmpty, text, error))
        return false;

    destination = text.toStdString();
    return true;
}

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

bool readEngineSampleRate (double engineSampleRate, juce::String& error)
{
    if (! std::isfinite (engineSampleRate)
        || engineSampleRate < 8000.0
        || engineSampleRate > 384000.0)
    {
        error = "unsupported engine sample rate";
        return false;
    }

    return true;
}

} // namespace

bool KitLoader::buildLoadedKit (const Kit& kit,
                                const juce::File& baseDirectory,
                                double engineSampleRate,
                                LoadedKit& destination,
                                juce::String& error)
{
    LoadedKit result;
    result.kit = kit;

    std::map<juce::String, std::shared_ptr<const SampleData>> cache;

    result.padBindings.reserve (kit.pads.size());

    for (std::size_t padIndex = 0; padIndex < kit.pads.size(); ++padIndex)
    {
        const auto& pad = kit.pads[padIndex];

        LoadedKit::PadBinding padBinding;
        padBinding.note = pad.note;
        padBinding.firstLayer = result.layerBindings.size();

        for (std::size_t layerIndex = 0; layerIndex < pad.layers.size(); ++layerIndex)
        {
            const auto& layer = pad.layers[layerIndex];

            LoadedKit::LayerBinding layerBinding;
            layerBinding.padIndex = padIndex;
            layerBinding.layerIndex = layerIndex;
            layerBinding.firstSample = result.samplePointers.size();
            layerBinding.sampleCount = layer.roundRobin.size();

            for (const auto& ref : layer.roundRobin)
            {
                const auto file = baseDirectory.getChildFile (juce::String (ref.path));
                const auto key = file.getFullPathName();

                auto found = cache.find (key);

                if (found == cache.end())
                {
                    juce::String loadError;
                    auto sample = SampleLoader::loadBlocking (file, engineSampleRate, loadError);

                    if (sample == nullptr)
                    {
                        error = "pad " + juce::String (pad.note) + ": sample \""
                                + juce::String (ref.path) + "\": "
                                + (loadError.isNotEmpty() ? loadError
                                                          : juce::String ("load failed"));
                        return false;
                    }

                    found = cache.emplace (key, std::move (sample)).first;
                    result.samples.push_back (found->second);
                }

                result.samplePointers.push_back (found->second.get());
            }

            result.layerBindings.push_back (layerBinding);
        }

        padBinding.numLayers = result.layerBindings.size() - padBinding.firstLayer;
        result.padBindings.push_back (padBinding);
    }

    result.numRoundRobinCounters = result.layerBindings.size();

    if (result.numRoundRobinCounters > 0)
    {
        result.roundRobinCounters =
            std::make_unique<std::atomic<std::uint32_t>[]> (result.numRoundRobinCounters);

        for (std::size_t i = 0; i < result.numRoundRobinCounters; ++i)
            result.roundRobinCounters[i].store (0, std::memory_order_relaxed);
    }

    destination = std::move (result);
    return true;
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

    if (! hasOnlyProperties (object, { "schemaVersion", "name", "pads" },
                             "kit", error))
    {
        return false;
    }

    const juce::var* versionValue = nullptr;
    const juce::var* nameValue = nullptr;
    const juce::var* padsValue = nullptr;

    if (! getRequired (object, "schemaVersion", "kit", versionValue, error)
        || ! getRequired (object, "name", "kit", nameValue, error)
        || ! getRequired (object, "pads", "kit", padsValue, error))
    {
        return false;
    }

    Kit result;

    if (! readInteger (*versionValue, "kit.schemaVersion", result.schemaVersion, error))
        return false;

    juce::String name;

    if (! readString (*nameValue, "kit.name", false, name, error))
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

bool KitLoader::loadFromJson (const juce::String& json,
                              const juce::File& baseDirectory,
                              double engineSampleRate,
                              LoadedKit& destination,
                              juce::String& error)
{
    error.clear();

    if (! readEngineSampleRate (engineSampleRate, error))
        return false;

    Kit parsed;

    if (! parse (json, parsed, error))
        return false;

    const auto base = baseDirectory == juce::File()
                          ? juce::File::getCurrentWorkingDirectory()
                          : baseDirectory;

    return buildLoadedKit (parsed, base, engineSampleRate, destination, error);
}

bool KitLoader::loadFromFile (const juce::File& file,
                              double engineSampleRate,
                              LoadedKit& destination,
                              juce::String& error)
{
    error.clear();

    if (! file.existsAsFile())
    {
        error = "Kit file not found: " + file.getFullPathName();
        return false;
    }

    if (file.getSize() > maximumKitBytes)
    {
        error = "Kit file is larger than the "
                + juce::String (maximumKitBytes) + " byte limit";
        return false;
    }

    juce::String json;

    {
        juce::FileInputStream stream (file);

        if (! stream.openedOk())
        {
            error = "Cannot read kit file: " + file.getFullPathName();
            return false;
        }

        json = stream.readEntireStreamAsString();
    }

    return loadFromJson (json, file.getParentDirectory(), engineSampleRate,
                         destination, error);
}

} // namespace ihub::sampler
