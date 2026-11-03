#include "api/routes/RouteHelpers.h"

namespace Api::Routes
{

namespace
{

ProjectOverview readOverview (const RouteContext& context)
{
    if (context.projectProvider == nullptr)
        return {};

    return context.projectProvider();
}

}

void registerProjectRoutes (httplib::Server& server, const RouteContext& context)
{
    server.Get ("/project", [context] (const httplib::Request&, httplib::Response& response)
    {
        decorate (context, response);
        setJson (response, 200, projectOverviewToVar (readOverview (context)));
    });

    server.Post ("/project", [context] (const httplib::Request& request,
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
        const auto action = object != nullptr ? object->getProperty ("action")
                                              : juce::var {};

        if (! action.isString() || action.toString().isEmpty())
        {
            setError (response, 400, "body must contain \"action\"");
            return;
        }

        ProjectCommand command;
        command.action = action.toString().toStdString();

        if (object->hasProperty ("path"))
        {
            if (! readString (object->getProperty ("path"), "path", command.path, error))
            {
                setError (response, 400, error);
                return;
            }
        }

        if (object->hasProperty ("name"))
        {
            if (! readString (object->getProperty ("name"), "name", command.name, error))
            {
                setError (response, 400, error);
                return;
            }
        }

        if (context.projectCommand == nullptr)
        {
            setError (response, 400, "project lifecycle is unavailable");
            return;
        }

        if (! context.projectCommand (command, error))
        {
            setError (response, 400, error);
            return;
        }

        decorate (context, response);
        setJson (response, 200, projectOverviewToVar (readOverview (context)));
    });
}

}
