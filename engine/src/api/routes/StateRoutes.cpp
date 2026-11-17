#include "api/routes/RouteHelpers.h"

#include <limits>

namespace Api::Routes {

void registerStateRoutes(httplib::Server& server, const RouteContext& context) {
  server.Get("/state",
             [context](const httplib::Request&, httplib::Response& response) {
               decorate(context, response);
               setJson(response, 200, stateToVar(context.state.snapshot()));
             });

  server.Post("/arm", [context](const httplib::Request& request,
                                httplib::Response& response) {
    juce::var body;
    std::string error;

    if (!parseJsonObject(request, body, error)) {
      setError(response, 400, error);
      return;
    }

    auto* object = body.getDynamicObject();
    juce::var value = object->getProperty("slot");

    if (value.isVoid()) value = object->getProperty("armedSlot");

    if (!object->hasProperty("slot") && !object->hasProperty("armedSlot")) {
      setError(response, 400,
               "body must contain \"slot\" (a number, or null to disarm)");
      return;
    }

    int slot = -1;

    if (!value.isVoid()) {
      std::int64_t parsed = 0;

      if (!detail::readInteger(value, "slot", -1,
                               std::numeric_limits<int>::max(), parsed,
                               error)) {
        setError(response, 400, error);
        return;
      }

      slot = static_cast<int>(parsed);
    }

    if (!context.state.setArmedSlot(slot)) {
      setError(response, 400, "slot is out of range");
      return;
    }

    decorate(context, response);
    setJson(response, 200, stateToVar(context.state.snapshot()));
  });

  server.Post(
      R"(/pages/([^/]+)/restart)",
      [context](const httplib::Request& request, httplib::Response& response) {
        const std::string pageId = request.matches[1].str();
        std::string error;

        if (!context.state.restartPage(pageId, error)) {
          const int status =
              error.find("no audio session") != std::string::npos ? 503 : 404;
          setError(response, status, error);
          return;
        }

        decorate(context, response);
        setJson(response, 200, stateToVar(context.state.snapshot()));
      });

  server.Get("/settings", [context](const httplib::Request&,
                                    httplib::Response& response) {
    decorate(context, response);
    setJson(response, 200, settingsToVar(context.state.getSettings()));
  });

  server.Post("/settings", [context](const httplib::Request& request,
                                     httplib::Response& response) {
    juce::var body;
    std::string error;

    if (!parseJsonObject(request, body, error)) {
      setError(response, 400, error);
      return;
    }

    if (!context.state.updateSettings(body, error)) {
      setError(response, 400, error);
      return;
    }

    decorate(context, response);
    setJson(response, 200, settingsToVar(context.state.getSettings()));
  });
}

}  // namespace Api::Routes
