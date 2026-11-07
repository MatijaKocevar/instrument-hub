import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { toolResult } from "../format.js";
import { destructive } from "./annotations.js";

export function registerPageTools(server: McpServer, api: HubApi): void {
  server.registerTool(
    "ihub_restart_page",
    {
      title: "Restart a shared plugin page",
      description:
        "Stop and restart the shared plugin host for a page, restore every slot's committed state and re-arm the previous track. Recovery can take up to 180 seconds and may stall on the first attempt (BUG-029); avoid timed loops.",
      inputSchema: {
        page: z
          .string()
          .trim()
          .min(1)
          .describe("Page id from GET /state, for example tracks."),
      },
      annotations: destructive,
    },
    ({ page }) => toolResult(() => api.restartPage(page)),
  );
}
