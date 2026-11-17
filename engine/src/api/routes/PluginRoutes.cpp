#include "api/routes/RouteHelpers.h"

namespace Api::Routes {

namespace {
juce::var pluginInventoryToVar(const PluginInventoryState& inventory) {
  auto root = makeObject();
  auto* object = root.getDynamicObject();

  juce::Array<juce::var> plugins;

  for (const auto& plugin : inventory.plugins) {
    auto item = makeObject();
    auto* itemObject = item.getDynamicObject();

    itemObject->setProperty("id", juce::String(plugin.id));
    itemObject->setProperty("name", juce::String(plugin.name));
    itemObject->setProperty("description", juce::String(plugin.description));
    itemObject->setProperty("vendor", juce::String(plugin.vendor));
    itemObject->setProperty("version", juce::String(plugin.version));
    itemObject->setProperty("category", juce::String(plugin.category));
    itemObject->setProperty("isInstrument", plugin.isInstrument);
    itemObject->setProperty("isEffect", plugin.isEffect);
    itemObject->setProperty("format", juce::String(plugin.format));
    itemObject->setProperty("path", juce::String(plugin.path));
    plugins.add(item);
  }

  object->setProperty("plugins", juce::var(plugins));
  object->setProperty("count", static_cast<int>(inventory.plugins.size()));
  object->setProperty("total", inventory.total);
  object->setProperty("search", juce::String(inventory.query));
  object->setProperty("scanning", inventory.scanning);
  object->setProperty("scanned", inventory.scanned);
  object->setProperty("lastScanMs",
                      static_cast<juce::int64>(inventory.lastScanMs));
  object->setProperty("error", juce::String(inventory.error));

  return root;
}
}  // namespace

void registerPluginRoutes(httplib::Server& server,
                          const RouteContext& context) {
  server.Get("/plugins", [context](const httplib::Request& request,
                                   httplib::Response& response) {
    std::string search;

    if (request.has_param("search"))
      search = request.get_param_value("search");
    else if (request.has_param("q"))
      search = request.get_param_value("q");

    PluginInventoryState inventory;

    try {
      if (context.pluginProvider != nullptr)
        inventory = context.pluginProvider(search);
    } catch (...) {
      setError(response, 500, "the plug-in inventory failed");
      return;
    }

    inventory.query = search;

    decorate(context, response);
    setJson(response, 200, pluginInventoryToVar(inventory));
  });

  server.Post("/plugins/rescan",
              [context](const httplib::Request&, httplib::Response& response) {
                if (context.pluginRescan == nullptr) {
                  setError(response, 503, "plugin inventory is not available");
                  return;
                }

                bool started = false;

                try {
                  started = context.pluginRescan();
                } catch (...) {
                  setError(response, 500, "could not start the plugin scan");
                  return;
                }

                PluginInventoryState inventory;

                try {
                  if (context.pluginProvider != nullptr)
                    inventory = context.pluginProvider({});
                } catch (...) {
                  setError(response, 500, "the plug-in inventory failed");
                  return;
                }

                auto body = pluginInventoryToVar(inventory);
                body.getDynamicObject()->setProperty("started", started);

                decorate(context, response);
                setJson(response, started ? 202 : 200, body);
              });
}

}  // namespace Api::Routes
