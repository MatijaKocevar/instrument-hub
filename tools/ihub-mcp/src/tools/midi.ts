import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { toolResult } from "../format.js";
import { stateChanging } from "./annotations.js";

export function registerMidiTools(server: McpServer, api: HubApi): void {
  server.registerTool(
    "ihub_set_midi_input",
    {
      title: "Change an Instrument Hub MIDI input",
      description:
        "Enable or disable one MIDI input, or change its role, note base or pad learn. Provide at least one change. Returns the MIDI state after the change.",
      inputSchema: {
        id: z
          .string()
          .trim()
          .min(1)
          .describe("Input id from GET /midi, for example 16-0."),
        enabled: z
          .boolean()
          .optional()
          .describe("Desired enable switch state."),
        role: z
          .enum(["keyboard", "grid"])
          .optional()
          .describe("keyboard plays the armed slot; grid drives the pad grid."),
        noteBase: z
          .number()
          .int()
          .min(-1)
          .max(64)
          .optional()
          .describe("First note of a sequential 64-pad map; -1 clears it."),
        learn: z
          .boolean()
          .optional()
          .describe("Start or cancel a 64-pad learn on a grid input."),
      },
      annotations: stateChanging,
    },
    ({ id, enabled, role, noteBase, learn }) =>
      toolResult(() => {
        if (
          enabled === undefined &&
          role === undefined &&
          noteBase === undefined &&
          learn === undefined
        ) {
          throw new Error(
            "Provide at least one of enabled, role, noteBase or learn.",
          );
        }
        return api.setMidiInput({
          id,
          ...(enabled !== undefined ? { enabled } : {}),
          ...(role !== undefined ? { role } : {}),
          ...(noteBase !== undefined ? { noteBase } : {}),
          ...(learn !== undefined ? { learn } : {}),
        });
      }),
  );
}
