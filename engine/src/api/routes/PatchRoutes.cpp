#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api::Routes
{

namespace
{
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
            if (! detail::readNumber (item, "frequency", 20.0, 20000.0,
                                      source.frequency, error))
                return false;
        }
        else if (key == "inputChannel")
        {
            std::int64_t parsed = 0;

            if (! detail::readInteger (item, "inputChannel", 0, 63, parsed, error))
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

    if (source.kind != "empty"
        && source.kind != "init-tone"
        && source.kind != "sampler"
        && source.kind != "plugin"
        && source.kind != "audio-input")
    {
        error = "kind must be empty, init-tone, sampler, plugin or audio-input";
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
}

void registerPatchRoutes (httplib::Server& server, const RouteContext& context)
{

    server.Patch (R"(/patches/(\d+))",
                  [context] (const httplib::Request& request,
                             httplib::Response& response)
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

        if (! context.state.updatePatch (slot, body, error))
        {
            setError (response, 400, error);
            return;
        }

        decorate (context, response);
        setJson (response, 200, stateToVar (context.state.snapshot()));
    });

    server.Post (R"(/patches/(\d+)/source)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
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

        PatchSourceRequest source;

        if (! parsePatchSource (body, source, error))
        {
            setError (response, 400, error);
            return;
        }

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

        decorate (context, response);
        setJson (response, 200, stateToVar (context.state.snapshot()));
    });

    const auto respondEditor = [context] (const httplib::Request& request,
                                          httplib::Response& response,
                                          bool open)
    {
        const int slot = readPatchSlot (context, request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        const bool wasOpen = context.state.getPatch (slot).editorOpen;
        std::string error;

        const bool applied = open ? context.state.openPatchEditor (slot, error)
                                  : context.state.closePatchEditor (slot, error);

        if (! applied)
        {
            const int status = error.find ("no audio session") != std::string::npos
                                   ? 503
                                   : 400;
            setError (response, status, error);
            return;
        }

        auto body = makeObject();
        body.getDynamicObject()->setProperty ("slot", slot);
        body.getDynamicObject()->setProperty ("open", open);

        if (open)
        {
            body.getDynamicObject()->setProperty ("reused", wasOpen);
            body.getDynamicObject()->setProperty ("wasOpen", wasOpen);
        }
        else
        {
            body.getDynamicObject()->setProperty ("wasOpen", wasOpen);
        }

        decorate (context, response);
        setJson (response, 200, body);
    };

    server.Post (R"(/patches/(\d+)/editor)",
                 [respondEditor] (const httplib::Request& request,
                                  httplib::Response& response)
    {
        respondEditor (request, response, true);
    });

    server.Delete (R"(/patches/(\d+)/editor)",
                   [respondEditor] (const httplib::Request& request,
                                    httplib::Response& response)
    {
        respondEditor (request, response, false);
    });

    server.Post (R"(/patches/(\d+)/state/capture)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
    {
        const int slot = readPatchSlot (context, request);

        if (slot < 0)
        {
            setError (response, 404, "patch slot is out of range");
            return;
        }

        PatchStateCapture capture;
        std::string error;

        if (! context.state.capturePatchState (slot, capture, error))
        {
            const int status = error.find ("no audio session") != std::string::npos
                                   ? 503
                                   : 400;
            setError (response, status, error);
            return;
        }

        auto body = makeObject();
        body.getDynamicObject()->setProperty ("slot", capture.slot);
        body.getDynamicObject()->setProperty ("captured", capture.captured);
        body.getDynamicObject()->setProperty ("sizeBytes",
                                              static_cast<juce::int64> (capture.sizeBytes));

        decorate (context, response);
        setJson (response, 200, body);
    });
}

}
