import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { toolResult } from "../format.js";
import { stateChanging, toggling } from "./annotations.js";

const trackIndex = z
  .number()
  .int()
  .min(0)
  .max(7)
  .describe("Track index, 0-7.");

export function registerTrackTools(server: McpServer, api: HubApi): void {
  server.registerTool(
    "ihub_arm_track",
    {
      title: "Arm an Instrument Hub track",
      description:
        "Select a performance track (0-7) and arm its slot. Only available in the tracks layout.",
      inputSchema: { index: trackIndex },
      annotations: stateChanging,
    },
    ({ index }) => toolResult(() => api.selectTrack(index)),
  );

  server.registerTool(
    "ihub_arm_slot",
    {
      title: "Arm an Instrument Hub slot",
      description:
        "Arm one audio slot (0-63), or disarm every slot with null or -1. Returns the state after the arm.",
      inputSchema: {
        slot: z
          .number()
          .int()
          .min(-1)
          .max(63)
          .nullable()
          .describe("Slot to arm (0-63); null or -1 disarms."),
      },
      annotations: stateChanging,
    },
    ({ slot }) =>
      toolResult(() => api.armSlot(slot === null || slot < 0 ? null : slot)),
  );

  server.registerTool(
    "ihub_set_track_volume",
    {
      title: "Set an Instrument Hub track volume",
      description:
        "Set a track's stepped volume: 0 silent, 1 50%, 2 75%, 3 100% (0 dB), 4 +3 dB. Returns the state after the change.",
      inputSchema: {
        index: trackIndex,
        step: z.number().int().min(0).max(4).describe("Volume step, 0-4."),
      },
      annotations: stateChanging,
    },
    ({ index, step }) => toolResult(() => api.setTrackVolume(index, step)),
  );

  server.registerTool(
    "ihub_toggle_track_fx",
    {
      title: "Toggle an Instrument Hub track FX",
      description:
        "Toggle bypass on a track's hot FX insert (hot 0 or 1). The insert must be assigned. Returns the state after the toggle.",
      inputSchema: {
        index: trackIndex,
        hot: z.number().int().min(0).max(1).describe("Hot FX slot, 0 or 1."),
      },
      annotations: toggling,
    },
    ({ index, hot }) => toolResult(() => api.toggleTrackFx(index, hot)),
  );
}
