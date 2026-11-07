import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { toolResult } from "../format.js";
import { destructive, stateChanging } from "./annotations.js";

const slot = z
  .number()
  .int()
  .min(0)
  .max(63)
  .describe("Patch slot, 0-63.");

export function registerPatchTools(server: McpServer, api: HubApi): void {
  server.registerTool(
    "ihub_open_editor",
    {
      title: "Open a patch's plugin editor",
      description:
        "Open the native editor window of the plugin hosted by a patch, or focus the existing window. The patch must host a resident plugin.",
      inputSchema: { slot },
      annotations: stateChanging,
    },
    ({ slot: target }) => toolResult(() => api.openPatchEditor(target)),
  );

  server.registerTool(
    "ihub_close_editor",
    {
      title: "Close a patch's plugin editor",
      description:
        "Close the native editor window of the plugin hosted by a patch. Idempotent when no window is open.",
      inputSchema: { slot },
      annotations: stateChanging,
    },
    ({ slot: target }) => toolResult(() => api.closePatchEditor(target)),
  );

  server.registerTool(
    "ihub_capture_patch_state",
    {
      title: "Capture a patch's plugin state",
      description:
        "Save the resident plugin's current sound into the patch state blob without switching away. The patch must host a resident plugin.",
      inputSchema: { slot },
      annotations: stateChanging,
    },
    ({ slot: target }) => toolResult(() => api.capturePatchState(target)),
  );

  server.registerTool(
    "ihub_reload_plugin",
    {
      title: "Reload a patch's plugin instance",
      description:
        "Restart one slot's plugin instance from its last committed state. Other slots are untouched; unsaved state in this patch is discarded.",
      inputSchema: { slot },
      annotations: destructive,
    },
    ({ slot: target }) => toolResult(() => api.reloadPatch(target)),
  );
}
