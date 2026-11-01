#include "project/serializer/SetPatch.h"

#include "project/serializer/JsonFields.h"

#include <cmath>
#include <utility>

namespace ihub::project::serializer
{

using juce::var;

namespace
{

var instrumentToVar (const InstrumentConfig& instrument)
{
    auto value = makeObject();
    auto* object = value.getDynamicObject();

    object->setProperty ("kind", juce::String (toString (instrument.kind)));
    object->setProperty ("id", juce::String (instrument.id));
    object->setProperty ("name", juce::String (instrument.name));
    object->setProperty ("pluginState", juce::String (instrument.pluginState));
    object->setProperty ("pluginFormat", juce::String (instrument.pluginFormat));
    object->setProperty ("pluginId", juce::String (instrument.pluginId));
    object->setProperty ("pluginName", juce::String (instrument.pluginName));
    object->setProperty ("pluginManufacturer", juce::String (instrument.pluginManufacturer));
    object->setProperty ("pluginVersion", juce::String (instrument.pluginVersion));
    object->setProperty ("samplePath", juce::String (instrument.samplePath));
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
        || ! readString (value, "pluginFormat", context, instrument.pluginFormat, error, false)
        || ! readString (value, "pluginId", context, instrument.pluginId, error, false)
        || ! readString (value, "pluginName", context, instrument.pluginName, error, false)
        || ! readString (value, "pluginManufacturer", context,
                         instrument.pluginManufacturer, error, false)
        || ! readString (value, "pluginVersion", context, instrument.pluginVersion, error, false)
        || ! readString (value, "samplePath", context, instrument.samplePath, error, false)
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

} // namespace

var slotToVar (const SlotConfig& slot)
{
    auto value = makeObject();
    auto* object = value.getDynamicObject();

    object->setProperty ("index", slot.index);
    object->setProperty ("name", juce::String (slot.name));
    object->setProperty ("gain", static_cast<double> (slot.gain));
    object->setProperty ("colour", juce::String (slot.colour));
    object->setProperty ("colorIndex", slot.colorIndex);
    object->setProperty ("pan", static_cast<double> (slot.pan));
    object->setProperty ("solo", slot.solo);
    object->setProperty ("active", slot.active);
    object->setProperty ("muted", slot.muted);
    object->setProperty ("instrument", instrumentToVar (slot.instrument));

    juce::Array<var> fxChain;

    for (const auto& fx : slot.fxChain)
        fxChain.add (fxToVar (fx));

    object->setProperty ("fx", juce::Array<var> (fxChain));
    return value;
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
    double pan = static_cast<double> (slot.pan);

    if (! readInteger (value, "index", context, slot.index, 0, 63, error)
        || ! readString (value, "name", context, slot.name, error, false)
        || ! readNumber (value, "gain", context, gain, error, false)
        || ! readString (value, "colour", context, slot.colour, error, false)
        || ! readInteger (value, "colorIndex", context, slot.colorIndex, 0,
                          maximumSlotColourIndex, error, false)
        || ! readNumber (value, "pan", context, pan, error, false)
        || ! readBool (value, "solo", context, slot.solo, error, false)
        || ! readBool (value, "active", context, slot.active, error, false)
        || ! readBool (value, "muted", context, slot.muted, error, false))
        return false;

    slot.gain = static_cast<float> (gain);
    slot.pan = static_cast<float> (pan);

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

} // namespace ihub::project::serializer
