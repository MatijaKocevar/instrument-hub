import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { toolResult } from "../format.js";
import { stateChanging } from "./annotations.js";

export function registerTransportTools(
  server: McpServer,
  api: HubApi,
): void {
  server.registerTool(
    "ihub_set_transport",
    {
      title: "Change the Instrument Hub transport",
      description:
        "Start, stop or reposition the transport and set the BPM. Provide at least one field. Returns the transport after the update.",
      inputSchema: {
        playing: z.boolean().optional().describe("Start or stop playback."),
        bpm: z
          .number()
          .min(20)
          .max(300)
          .optional()
          .describe("Tempo in beats per minute, 20-300."),
        ppq: z
          .number()
          .min(0)
          .optional()
          .describe("Quarter-note position since the last rewind."),
        positionSamples: z
          .number()
          .int()
          .min(0)
          .optional()
          .describe("Absolute position in engine sample frames."),
      },
      annotations: stateChanging,
    },
    (patch) =>
      toolResult(() => {
        if (
          patch.playing === undefined &&
          patch.bpm === undefined &&
          patch.ppq === undefined &&
          patch.positionSamples === undefined
        ) {
          throw new Error("Provide at least one transport field.");
        }
        return api.setTransport({
          ...(patch.playing !== undefined ? { playing: patch.playing } : {}),
          ...(patch.bpm !== undefined ? { bpm: patch.bpm } : {}),
          ...(patch.ppq !== undefined ? { ppq: patch.ppq } : {}),
          ...(patch.positionSamples !== undefined
            ? { positionSamples: patch.positionSamples }
            : {}),
        });
      }),
  );
}
