#include "api/routes/RouteHelpers.h"

#include <cstdint>
#include <cstdlib>

namespace Api::Routes
{

namespace
{

bool readDialogId (const httplib::Request& request, std::uint64_t& id)
{
    if (request.matches.size() <= 1)
        return false;

    const auto text = request.matches[1].str();
    char* end = nullptr;
    const auto parsed = std::strtoull (text.c_str(), &end, 10);

    if (end == nullptr || *end != '\0')
        return false;

    id = static_cast<std::uint64_t> (parsed);
    return true;
}

}

void registerProjectDialogRoutes (httplib::Server& server,
                                  const RouteContext& context)
{
    server.Post ("/project/dialog", [context] (const httplib::Request& request,
                                               httplib::Response& response)
    {
        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        const auto* object = body.getDynamicObject();
        ProjectDialogRequest dialog;
        const auto mode = object->getProperty ("mode");

        if (! mode.isString()
            || ! readString (mode, "mode", dialog.mode, error))
        {
            setError (response, 400,
                      error.empty() ? "body must contain \"mode\"" : error);
            return;
        }

        if (dialog.mode != "open" && dialog.mode != "save")
        {
            setError (response, 400, "mode must be \"open\" or \"save\"");
            return;
        }

        if (object->hasProperty ("currentPath")
            && ! readString (object->getProperty ("currentPath"), "currentPath",
                             dialog.currentPath, error))
        {
            setError (response, 400, error);
            return;
        }

        if (object->hasProperty ("suggestedName")
            && ! readString (object->getProperty ("suggestedName"),
                             "suggestedName", dialog.suggestedName, error))
        {
            setError (response, 400, error);
            return;
        }

        if (context.projectDialogBegin == nullptr)
        {
            setError (response, 400, "file dialogs are unavailable");
            return;
        }

        std::uint64_t id = 0;

        if (! context.projectDialogBegin (dialog, id, error))
        {
            setError (response, 400, error);
            return;
        }

        ProjectDialogStatus status;
        status.id = id;
        status.state = "pending";

        decorate (context, response);
        setJson (response, 200, projectDialogStatusToVar (status));
    });

    server.Get (R"(/project/dialog/(\d+))",
                [context] (const httplib::Request& request,
                           httplib::Response& response)
    {
        std::uint64_t id = 0;

        if (! readDialogId (request, id))
        {
            setError (response, 400, "invalid dialog id");
            return;
        }

        if (context.projectDialogPoll == nullptr)
        {
            setError (response, 400, "file dialogs are unavailable");
            return;
        }

        ProjectDialogStatus status;
        std::string error;

        if (! context.projectDialogPoll (id, status, error))
        {
            setError (response, 404, error);
            return;
        }

        decorate (context, response);
        setJson (response, 200, projectDialogStatusToVar (status));
    });
}

}
