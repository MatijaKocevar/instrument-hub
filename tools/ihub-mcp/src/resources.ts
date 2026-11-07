import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import type { HubApi } from "./api/client.js";
import { formatJson } from "./format.js";

export function registerResources(server: McpServer, api: HubApi): void {
  registerJsonResource(server, {
    name: "state",
    uri: "ihub://state",
    title: "Instrument Hub state",
    description: "Complete control state, equivalent to GET /state.",
    load: () => api.getState(),
  });

  registerJsonResource(server, {
    name: "project",
    uri: "ihub://project",
    title: "Instrument Hub project",
    description: "Open project and recent project list, equivalent to GET /project.",
    load: () => api.getProject(),
  });

  registerJsonResource(server, {
    name: "audio",
    uri: "ihub://audio",
    title: "Instrument Hub audio",
    description: "Audio device, latency and xrun state, equivalent to GET /audio.",
    load: () => api.getAudio(),
  });

  registerJsonResource(server, {
    name: "midi",
    uri: "ihub://midi",
    title: "Instrument Hub MIDI",
    description: "MIDI inputs and controller roles, equivalent to GET /midi.",
    load: () => api.getMidi(),
  });

  registerJsonResource(server, {
    name: "plugins",
    uri: "ihub://plugins",
    title: "Instrument Hub plugins",
    description: "Installed VST3 plugin inventory, equivalent to GET /plugins.",
    load: () => api.getPlugins(),
  });
}

interface JsonResource {
  name: string;
  uri: string;
  title: string;
  description: string;
  load: () => Promise<unknown>;
}

function registerJsonResource(server: McpServer, resource: JsonResource): void {
  server.registerResource(
    resource.name,
    resource.uri,
    {
      title: resource.title,
      description: resource.description,
      mimeType: "application/json",
    },
    async (uri) => ({
      contents: [
        {
          uri: uri.href,
          mimeType: "application/json",
          text: formatJson(await resource.load()),
        },
      ],
    }),
  );
}
