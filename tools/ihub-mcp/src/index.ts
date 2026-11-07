#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { createHubApi } from "./api/client.js";
import { loadConfig } from "./config.js";
import { registerResources } from "./resources.js";
import { registerMidiTools } from "./tools/midi.js";
import { registerReadTools } from "./tools/read.js";
import { registerTrackTools } from "./tools/tracks.js";
import { registerTransportTools } from "./tools/transport.js";

async function main(): Promise<void> {
  const config = loadConfig();
  const api = createHubApi(config);
  const server = new McpServer({
    name: "instrument-hub",
    version: "0.1.0",
  });

  registerReadTools(server, api, config);
  registerTrackTools(server, api);
  registerTransportTools(server, api);
  registerMidiTools(server, api);
  registerResources(server, api);

  await server.connect(new StdioServerTransport());
}

main().catch((error: unknown) => {
  const message = error instanceof Error ? error.message : String(error);
  process.stderr.write(`ihub-mcp: ${message}\n`);
  process.exitCode = 1;
});
