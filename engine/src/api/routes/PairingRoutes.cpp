#include "api/routes/RouteHelpers.h"

namespace Api::Routes
{

void registerPairingRoutes (httplib::Server& server, const RouteContext& context)
{

    server.Get ("/pairing", [context] (const httplib::Request&, httplib::Response& response)
    {
        if (context.pairing == nullptr)
        {
            setError (response, 503, "pairing is not available");
            return;
        }

        decorate (context, response);
        setJson (response, 200, pairingToVar (context.pairing->snapshot()));
    });

    server.Post ("/pairing/lan", [context] (const httplib::Request& request,
                                            httplib::Response& response)
    {
        if (context.pairing == nullptr || context.setLanMode == nullptr)
        {
            setError (response, 503, "pairing is not available");
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

        if (! object->hasProperty ("enabled"))
        {
            setError (response, 400, "body must contain \"enabled\" (boolean)");
            return;
        }

        bool enabled = false;

        if (! detail::readBool (object->getProperty ("enabled"), "enabled", enabled, error))
        {
            setError (response, 400, error);
            return;
        }

        decorate (context, response);
        setJson (response, 200, pairingToVar (context.setLanMode (enabled)));
    });

    server.Post ("/pairing/revoke", [context] (const httplib::Request&,
                                               httplib::Response& response)
    {
        if (context.pairing == nullptr)
        {
            setError (response, 503, "pairing is not available");
            return;
        }

        context.pairing->revokeAll();

        decorate (context, response);
        setJson (response, 200, pairingToVar (context.pairing->snapshot()));
    });

    server.Post ("/pair", [context] (const httplib::Request& request,
                                     httplib::Response& response)
    {
        if (context.pairing == nullptr)
        {
            setError (response, 503, "pairing is not available");
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
        std::string code;
        std::string clientName;

        if (! object->hasProperty ("code"))
        {
            setError (response, 400, "body must contain \"code\"");
            return;
        }

        if (! readString (object->getProperty ("code"), "code", code, error))
        {
            setError (response, 400, error);
            return;
        }

        if (object->hasProperty ("clientName"))
        {
            if (! readString (object->getProperty ("clientName"), "clientName",
                              clientName, error))
            {
                setError (response, 400, error);
                return;
            }
        }

        std::string token;

        if (! context.pairing->pair (code, token, error, std::move (clientName)))
        {
            setError (response, context.pairing->isLanEnabled() ? 401 : 403, error);
            return;
        }

        auto result = makeObject();
        result.getDynamicObject()->setProperty ("token", juce::String (token));
        result.getDynamicObject()->setProperty ("pairedClients",
                                                static_cast<int> (context.pairing->pairedClientCount()));

        decorate (context, response);
        setJson (response, 200, result);
    });
}

}
