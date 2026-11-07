import { isAbsolute } from "node:path";
import type { components } from "./api/schema.js";

type Plugin = components["schemas"]["Plugin"];
type PatchSourceRequest = components["schemas"]["PatchSourceRequest"];

export function resolvePluginSource(
  plugins: readonly Plugin[],
  query: string,
  vendor?: string,
): PatchSourceRequest {
  const matches = matchPlugins(plugins, query, vendor);
  if (matches.length === 0) {
    throw new Error(
      `No installed plugin matches "${query}"${vendor ? ` from ${vendor}` : ""}. ` +
        "Use ihub_plugins to find its id or exact display name; bundle paths are not accepted.",
    );
  }
  if (matches.length > 1) {
    const names = matches.map((plugin) => `${plugin.name} (${plugin.vendor})`);
    throw new Error(
      `"${query}" matches ${matches.length} installed plugins: ${names.join(", ")}. ` +
        "Pass the plugin id or a vendor to choose one.",
    );
  }

  const plugin = matches[0]!;
  if (!isAbsolute(plugin.path)) {
    throw new Error(
      `The plugin inventory returned a non-absolute bundle path for ${plugin.name}.`,
    );
  }

  return {
    kind: "plugin",
    name: plugin.name,
    path: plugin.path,
    pluginFormat: plugin.format,
    pluginId: plugin.id,
    pluginName: plugin.name,
    pluginManufacturer: plugin.vendor,
    pluginVersion: plugin.version,
  };
}

function matchPlugins(
  plugins: readonly Plugin[],
  query: string,
  vendor?: string,
): Plugin[] {
  const needle = query.trim();
  if (needle.length === 0) throw new Error("plugin must not be empty.");

  const byId = plugins.filter((plugin) => plugin.id === needle);
  const candidates =
    byId.length > 0
      ? byId
      : plugins.filter(
          (plugin) => plugin.name.toLowerCase() === needle.toLowerCase(),
        );

  if (vendor === undefined) return candidates;
  const wanted = vendor.trim().toLowerCase();
  return candidates.filter(
    (plugin) => plugin.vendor.toLowerCase() === wanted,
  );
}
