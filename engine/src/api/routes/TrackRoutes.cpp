#include "api/routes/RouteHelpers.h"

#include <cstdlib>
#include <string>

namespace Api::Routes
{

namespace
{
int readIndex (const httplib::Request& request, std::size_t match)
{
    if (request.matches.size() <= match)
        return -1;

    const auto text = request.matches[match].str();
    char* end = nullptr;
    const long parsed = std::strtol (text.c_str(), &end, 10);

    if (end == nullptr || *end != '\0' || parsed < 0)
        return -1;

    return static_cast<int> (parsed);
}

int statusForTrackError (const std::string& error)
{
    if (error.find ("no audio session") != std::string::npos)
        return 503;

    if (error.find ("out of range") != std::string::npos)
        return 404;

    return 400;
}

void respondState (const RouteContext& context, httplib::Response& response)
{
    decorate (context, response);
    setJson (response, 200, stateToVar (context.state.snapshot()));
}
}

void registerTrackRoutes (httplib::Server& server, const RouteContext& context)
{
    server.Post (R"(/tracks/(\d+)/select)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
    {
        const int index = readIndex (request, 1);
        std::string error;

        if (! context.state.selectTrack (index, error))
        {
            setError (response, statusForTrackError (error), error);
            return;
        }

        respondState (context, response);
    });

    server.Post (R"(/tracks/(\d+)/fx/(\d+)/toggle)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
    {
        const int index = readIndex (request, 1);
        const int hot = readIndex (request, 2);
        std::string error;

        if (! context.state.toggleTrackFx (index, hot, error))
        {
            setError (response, statusForTrackError (error), error);
            return;
        }

        respondState (context, response);
    });

    server.Post (R"(/tracks/(\d+)/fx/(\d+)/assign)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
    {
        const int index = readIndex (request, 1);
        const int hot = readIndex (request, 2);

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();

        if (! object->hasProperty ("insertId"))
        {
            setError (response, 400, "body must contain \"insertId\"");
            return;
        }

        std::string insertId;

        if (! readString (object->getProperty ("insertId"), "insertId",
                          insertId, error))
        {
            setError (response, 400, error);
            return;
        }

        if (! context.state.assignTrackFx (index, hot, insertId, error))
        {
            setError (response, statusForTrackError (error), error);
            return;
        }

        respondState (context, response);
    });

    server.Post (R"(/tracks/(\d+)/volume)",
                 [context] (const httplib::Request& request,
                            httplib::Response& response)
    {
        const int index = readIndex (request, 1);

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();

        if (! object->hasProperty ("step"))
        {
            setError (response, 400, "body must contain \"step\" (0..4)");
            return;
        }

        std::int64_t step = 0;

        if (! detail::readInteger (object->getProperty ("step"), "step",
                                   0, 5, step, error))
        {
            setError (response, 400, error);
            return;
        }

        if (! context.state.setTrackVolumeStep (index, static_cast<int> (step), error))
        {
            setError (response, statusForTrackError (error), error);
            return;
        }

        respondState (context, response);
    });
}

}
