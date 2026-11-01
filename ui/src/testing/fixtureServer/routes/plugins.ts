import { PLUGIN_FIXTURE } from "../../fixtures";
import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handlePlugins: FixtureRoute = (context, request, response) => {
  const url = request.url ?? "";

  if (request.method === "GET" && url.startsWith("/plugins")) {
    context.pluginRequests.push(url);

    if (context.pluginPendingScan) {
      context.pluginPendingScan = false;
      context.pluginScanning = false;

      if (
        !context.pluginPlugins.some(
          (plugin) => plugin.id === "com.instrumenthub.TestDelay",
        )
      ) {
        context.pluginPlugins = [
          ...context.pluginPlugins,
          {
            id: "com.instrumenthub.TestDelay",
            name: "TestDelay",
            description: "Instrument Hub test delay",
            vendor: "Instrument Hub",
            version: "1.0.0",
            category: "Fx",
            isInstrument: false,
            isEffect: true,
            format: "VST3",
            path: "/usr/lib/vst3/TestDelay.vst3",
          },
        ];
      }
    }

    const searchAt = url.indexOf("search=");
    const search =
      searchAt < 0
        ? ""
        : decodeURIComponent(
            url.slice(searchAt + "search=".length).split("&")[0] ?? "",
          ).toLowerCase();

    const plugins = context.pluginPlugins.filter(
      (plugin) =>
        search.length === 0 ||
        plugin.name.toLowerCase().includes(search) ||
        plugin.vendor.toLowerCase().includes(search) ||
        plugin.category.toLowerCase().includes(search) ||
        plugin.path.toLowerCase().includes(search),
    );

    sendJson(response, 200, {
      ...PLUGIN_FIXTURE,
      plugins,
      count: plugins.length,
      total: context.pluginPlugins.length,
      search,
      scanning: context.pluginScanning,
    });

    return true;
  }

  if (request.method === "POST" && url === "/plugins/rescan") {
    context.pluginRequests.push("rescan");
    context.pluginScanning = true;
    context.pluginPendingScan = true;

    sendJson(response, 202, {
      ...PLUGIN_FIXTURE,
      plugins: context.pluginPlugins,
      count: context.pluginPlugins.length,
      total: context.pluginPlugins.length,
      search: "",
      scanning: true,
      started: true,
    });

    return true;
  }

  return false;
};
