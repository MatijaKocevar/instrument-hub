#include "api/routes/RouteHelpers.h"

namespace Api::Routes {

void registerTransportRoutes(httplib::Server& server, const RouteContext& context) {
    server.Get("/transport", [context](const httplib::Request&, httplib::Response& response) {
        decorate(context, response);
        setJson(response, 200, transportToVar(context.state.getTransport()));
    });

    server.Post("/transport",
                [context](const httplib::Request& request, httplib::Response& response) {
                    juce::var body;
                    std::string error;

                    if (!parseJsonObject(request, body, error)) {
                        setError(response, 400, error);
                        return;
                    }

                    if (!context.state.updateTransport(body, error)) {
                        setError(response, 400, error);
                        return;
                    }

                    decorate(context, response);
                    setJson(response, 200, transportToVar(context.state.getTransport()));
                });
}

}  // namespace Api::Routes
