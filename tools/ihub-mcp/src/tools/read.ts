import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import type { HubApi } from "../api/client.js";
import { readLatestMeterFrame } from "../api/meters.js";
import type { HubConfig } from "../config.js";
import { toolResult } from "../format.js";
import { readOnlyTool } from "./annotations.js";
import { waitForPageReady } from "./wait.js";

export function registerReadTools(
  server: McpServer,
  api: HubApi,
  config: HubConfig,
): void {
  server.registerTool(
    "ihub_state",
    {
      title: "Read Instrument Hub state",
      description:
        "Read the complete Instrument Hub control state: slots, tracks, pages, settings and transport.",
      annotations: readOnlyTool,
    },
    () => toolResult(() => api.getState()),
  );

  server.registerTool(
    "ihub_project",
    {
      title: "Read Instrument Hub project",
      description:
        "Read the open .ihub project path, name, dirty/saved flags and recent projects.",
      annotations: readOnlyTool,
    },
    () => toolResult(() => api.getProject()),
  );

  server.registerTool(
    "ihub_audio",
    {
      title: "Read Instrument Hub audio state",
      description:
        "Read the audio device, sample rate, buffer size, latency, channels, profiles and xrun counters.",
      annotations: readOnlyTool,
    },
    () => toolResult(() => api.getAudio()),
  );

  server.registerTool(
    "ihub_midi",
    {
      title: "Read Instrument Hub MIDI inputs",
      description:
        "Read the MIDI inputs, controller roles and open/enabled flags.",
      annotations: readOnlyTool,
    },
    () => toolResult(() => api.getMidi()),
  );

  server.registerTool(
    "ihub_plugins",
    {
      title: "Read Instrument Hub plugin inventory",
      description:
        "List installed VST3 plugins, optionally filtered by a case-insensitive search string.",
      inputSchema: {
        search: z
          .string()
          .trim()
          .optional()
          .describe("Optional case-insensitive filter over plugin metadata."),
      },
      annotations: readOnlyTool,
    },
    ({ search }) => toolResult(() => api.getPlugins(search)),
  );

  server.registerTool(
    "ihub_meters",
    {
      title: "Read one Instrument Hub meter frame",
      description:
        "Read the latest meter frame from the /meters WebSocket stream and close the socket.",
      inputSchema: {
        timeoutMs: z
          .number()
          .int()
          .min(100)
          .max(30000)
          .default(5000)
          .describe("Maximum time to wait for a frame, in milliseconds."),
      },
      annotations: readOnlyTool,
    },
    ({ timeoutMs }) => toolResult(() => readLatestMeterFrame(config, timeoutMs)),
  );

  server.registerTool(
    "ihub_transport",
    {
      title: "Read Instrument Hub transport",
      description:
        "Read the transport playing state, BPM, PPQ position and sample position.",
      annotations: readOnlyTool,
    },
    () => toolResult(() => api.getTransport()),
  );

  server.registerTool(
    "ihub_wait_for_page_ready",
    {
      title: "Wait for a plugin page to be ready",
      description:
        "Poll the state until a shared plugin page reports status ready. Defaults to the first page.",
      inputSchema: {
        page: z
          .string()
          .trim()
          .optional()
          .describe("Page id to wait for. Defaults to the first page."),
        timeoutMs: z
          .number()
          .int()
          .min(100)
          .max(120000)
          .default(10000)
          .describe("Maximum wait, in milliseconds."),
      },
      annotations: readOnlyTool,
    },
    ({ page, timeoutMs }) =>
      toolResult(() => waitForPageReady(api, page, timeoutMs)),
  );
}
