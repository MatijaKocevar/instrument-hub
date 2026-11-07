import { isAbsolute } from "node:path";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import type { components } from "../api/schema.js";
import { toolResult } from "../format.js";
import { requireConfirmation } from "../security.js";
import { destructive } from "./annotations.js";

type ProjectCommand = components["schemas"]["ProjectCommand"];
type ProjectAction = "new" | "open" | "save" | "save-as";

const action = z
  .enum(["new", "open", "save", "save-as"])
  .describe("Project lifecycle action.");

export function registerProjectTools(server: McpServer, api: HubApi): void {
  server.registerTool(
    "ihub_project_action",
    {
      title: "Run an Instrument Hub project action",
      description:
        "Create, open, save or save-as a project. new and open replace the running session and require confirm: true; open and save-as require an absolute .ihub path. Returns the project overview after the action.",
      inputSchema: {
        action,
        path: z
          .string()
          .trim()
          .min(1)
          .optional()
          .describe("Absolute .ihub path for open and save-as."),
        name: z
          .string()
          .trim()
          .min(1)
          .optional()
          .describe("Display name for new."),
        confirm: z
          .boolean()
          .optional()
          .describe("Must be true for new and open."),
      },
      annotations: destructive,
    },
    (input) =>
      toolResult(async () => {
        if (input.action === "new" || input.action === "open") {
          requireConfirmation(input.confirm, `Project ${input.action}`);
        }
        return api.updateProject(buildCommand(input));
      }),
  );
}

interface ProjectInput {
  action: ProjectAction;
  path?: string | undefined;
  name?: string | undefined;
}

function buildCommand(input: ProjectInput): ProjectCommand {
  if (input.action === "open" || input.action === "save-as") {
    if (input.path === undefined) {
      throw new Error(`Project ${input.action} requires an absolute path.`);
    }
    assertAbsolute(input.path);
  }
  if (input.action === "new" && input.path !== undefined) {
    assertAbsolute(input.path);
  }
  if (input.action === "save" && input.path !== undefined) {
    throw new Error("Project save writes the open project; use save-as for a path.");
  }
  if (input.name !== undefined && input.action !== "new") {
    throw new Error('Project name is only valid with action "new".');
  }

  return {
    action: input.action,
    ...(input.path !== undefined ? { path: input.path } : {}),
    ...(input.name !== undefined ? { name: input.name } : {}),
  };
}

function assertAbsolute(path: string): void {
  if (!isAbsolute(path)) {
    throw new Error(`Project path must be absolute: ${path}`);
  }
}
