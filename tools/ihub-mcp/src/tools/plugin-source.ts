import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { toolResult } from "../format.js";
import { resolvePluginSource } from "../plugin-catalog.js";
import { requireConfirmation } from "../security.js";
import { destructive } from "./annotations.js";

const slot = z
  .number()
  .int()
  .min(0)
  .max(63)
  .describe("Patch slot, 0-63.");

export function registerPluginSourceTools(
  server: McpServer,
  api: HubApi,
): void {
  server.registerTool(
    "ihub_assign_plugin",
    {
      title: "Assign an installed plugin to a patch",
      description:
        "Replace a patch's instrument with an installed VST3. The plugin is resolved by id, or by exact display name plus optional vendor, from the live /plugins inventory; a bundle path is never accepted. Returns the state after the change.",
      inputSchema: {
        slot,
        plugin: z
          .string()
          .trim()
          .min(1)
          .describe(
            "Plugin id or exact display name from ihub_plugins, never a path.",
          ),
        vendor: z
          .string()
          .trim()
          .min(1)
          .optional()
          .describe("Exact vendor name used to disambiguate a display name."),
      },
      annotations: destructive,
    },
    ({ slot: target, plugin, vendor }) =>
      toolResult(async () => {
        const inventory = await api.getPlugins();
        const source = resolvePluginSource(inventory.plugins, plugin, vendor);
        return api.setPatchSource(target, source);
      }),
  );

  server.registerTool(
    "ihub_clear_source",
    {
      title: "Clear a patch's instrument source",
      description:
        "Replace a patch's instrument with the empty source. This discards the current sound and requires confirm: true. Returns the state after the change.",
      inputSchema: {
        slot,
        confirm: z
          .boolean()
          .optional()
          .describe("Must be true; clearing replaces the patch source."),
      },
      annotations: destructive,
    },
    ({ slot: target, confirm }) =>
      toolResult(async () => {
        requireConfirmation(confirm, "Clearing a patch source");
        return api.setPatchSource(target, { kind: "empty" });
      }),
  );
}
