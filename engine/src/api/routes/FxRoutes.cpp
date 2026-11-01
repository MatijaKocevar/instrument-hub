#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <map>

namespace Api::Routes
{

namespace
{
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
            if (! detail::readBool (item, "bypassed", insert.bypassed, error))
                return false;
        }
        else if (key == "wet")
        {
            if (! detail::readNumber (item, "wet", 0.0, 1.0, insert.wet, error))
                return false;
        }
        else if (key == "position")
        {
            std::int64_t parsed = 0;

            if (! detail::readInteger (item, "position", -1, 1024, parsed, error))
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

            if (! detail::readBool (item, "bypassed", parsed, error))
                return false;

            update.bypassed = parsed;
            any = true;
        }
        else if (key == "wet")
        {
            double parsed = 0.0;

            if (! detail::readNumber (item, "wet", 0.0, 1.0, parsed, error))
                return false;

            update.wet = parsed;
            any = true;
        }
        else if (key == "position")
        {
            std::int64_t parsed = 0;

            if (! detail::readInteger (item, "position", 0, 1024, parsed, error))
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
}

void registerFxRoutes (httplib::Server& server, const RouteContext& context)
{
    const auto respondFx = [context] (httplib::Response& response,
                                      const FxChainState& chain)
    {
        decorate (context, response);
        setJson (response, 200, fxChainToVar (chain));
    };

    const auto respondFxError = [] (httplib::Response& response, const std::string& error)
    {
        const int status = error.find ("no audio session") != std::string::npos ? 503 : 400;
        setError (response, status, error);
    };

    server.Get (R"(/patches/(\d+)/fx)",
                [context, respondFx, respondFxError] (
                    const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);

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
                 [context, respondFx, respondFxError] (
                     const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);

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
                [context, respondFx, respondFxError] (
                    const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);

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
                  [context, respondFx, respondFxError] (
                      const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);
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
                   [context, respondFx, respondFxError] (
                       const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);
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
                 [context, respondFx, respondFxError] (
                     const httplib::Request& request, httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);

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
}

}
