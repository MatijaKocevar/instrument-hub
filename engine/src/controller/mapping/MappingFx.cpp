#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>

namespace Controller::MappingDetail
{

bool readFx (const YamlValue& value, PatchFxSpec& fx, MappingError& error)
{
    if (value.kind != YamlKind::mapping)
        return fail (error, value, "an fx entry must be a mapping");

    if (! checkKeys (value, { "kind", "id", "name", "pluginState", "bypassed",
                              "parameters" }, error))
        return false;

    const auto* kindValue = value.find ("kind");
    const auto* idValue = value.find ("id");

    if (idValue == nullptr)
        return fail (error, value, "an fx entry needs an id");

    if (kindValue != nullptr && ! readString (*kindValue, fx.kind, error, "fx kind"))
        return false;

    if (fx.kind != "builtin" && fx.kind != "plugin")
        return fail (error, kindValue != nullptr ? *kindValue : value,
                     "fx kind must be 'builtin' or 'plugin'");

    if (! readString (*idValue, fx.id, error, "fx id"))
        return false;

    if (const auto* name = value.find ("name"))
        if (! readString (*name, fx.name, error, "fx name"))
            return false;

    if (const auto* state = value.find ("pluginState"))
        if (! readString (*state, fx.pluginState, error, "fx pluginState", true))
            return false;

    if (const auto* bypassed = value.find ("bypassed"))
        if (! readBool (*bypassed, fx.bypassed, error, "fx bypassed"))
            return false;

    if (const auto* parameters = value.find ("parameters"))
    {
        if (parameters->kind != YamlKind::mapping)
            return fail (error, *parameters, "fx parameters must be a mapping");

        for (const auto& [key, parameterValue] : parameters->mapping)
        {
            if (key.empty())
                return fail (error, parameterValue, "an fx parameter name must not be empty");

            float parsed = 0.0f;

            if (! readFloat (parameterValue, parsed, error, "fx parameter '" + key + "'"))
                return false;

            fx.parameters[key] = parsed;
        }
    }

    return true;
}

} // namespace Controller::MappingDetail
