#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { createHubApi } from "./api/client.js";
import { loadConfig } from "./config.js";
import { registerResources } from "./resources.js";
import { registerMidiTools } from "./tools/midi.js";
import { registerPageTools } from "./tools/pages.js";
import { registerPatchTools } from "./tools/patch.js";
import { registerPluginSourceTools } from "./tools/plugin-source.js";
import { registerProjectTools } from "./tools/project.js";
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
  registerResources(server, api);

  if (config.readOnly) {
    process.stderr.write(
      "ihub-mcp: IHUB_MCP_READ_ONLY is set; write tools are not registered\n",
    );
  } else {
    registerTrackTools(server, api);
    registerTransportTools(server, api);
    registerMidiTools(server, api);
    registerPluginSourceTools(server, api);
    registerPatchTools(server, api);
    registerPageTools(server, api);
    registerProjectTools(server, api);
  }

  await server.connect(new StdioServerTransport());
}

main().catch((error: unknown) => {
  const message = error instanceof Error ? error.message : String(error);
  process.stderr.write(`ihub-mcp: ${message}\n`);
  process.exitCode = 1;
});
