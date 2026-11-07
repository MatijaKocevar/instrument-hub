#!/usr/bin/env node
import assert from "node:assert/strict";
import { fileURLToPath } from "node:url";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import {
  getDefaultEnvironment,
  StdioClientTransport,
} from "@modelcontextprotocol/sdk/client/stdio.js";

const packageRoot = fileURLToPath(new URL("../", import.meta.url));
const timeout = setTimeout(() => {
  console.error("smoke: timed out after 30 s");
  process.exit(1);
}, 30000);

const readTools = [
  "ihub_state",
  "ihub_project",
  "ihub_audio",
  "ihub_midi",
  "ihub_plugins",
  "ihub_meters",
  "ihub_transport",
  "ihub_wait_for_page_ready",
];
const resources = [
  "ihub://state",
  "ihub://project",
  "ihub://audio",
  "ihub://midi",
  "ihub://plugins",
];

const env = {
  ...getDefaultEnvironment(),
  ...(process.env.IHUB_API_URL ? { IHUB_API_URL: process.env.IHUB_API_URL } : {}),
  ...(process.env.IHUB_TOKEN ? { IHUB_TOKEN: process.env.IHUB_TOKEN } : {}),
};

const client = new Client({ name: "ihub-mcp-smoke", version: "0.1.0" });
const transport = new StdioClientTransport({
  command: process.execPath,
  args: ["dist/index.js"],
  cwd: packageRoot,
  env,
  stderr: "inherit",
});

try {
  await client.connect(transport);
  const server = client.getServerVersion();
  assert.equal(server?.name, "instrument-hub");
  console.log(`initialized ${server.name} ${server.version}`);

  const { tools } = await client.listTools();
  const toolNames = tools.map((tool) => tool.name).sort();
  assert.deepEqual(toolNames, [...readTools].sort(), "only read-only tools registered");
  assert.ok(
    tools.every((tool) => tool.annotations?.readOnlyHint === true),
    "every tool is annotated read-only",
  );
  console.log(`tools: ${toolNames.join(", ")}`);

  const { resources: listed } = await client.listResources();
  const resourceUris = listed.map((resource) => resource.uri).sort();
  assert.deepEqual(resourceUris, [...resources].sort(), "read-only resources registered");
  console.log(`resources: ${resourceUris.join(", ")}`);

  const state = await callJson("ihub_state");
  assert.equal(state.pages?.[0]?.status, "ready", "pages[0].status is ready");
  assert.ok(Array.isArray(state.slots), "state has slots");
  console.log(`state: revision ${state.revision}, ${state.slots.length} slots, ${state.pages.length} pages`);

  const ready = await callJson("ihub_wait_for_page_ready", { timeoutMs: 5000 });
  assert.equal(ready.page?.status, "ready", "page reported ready");
  console.log(`page ready: ${ready.page.id} after ${ready.waitedMs} ms`);

  const project = await callJson("ihub_project");
  assert.equal(typeof project.path, "string");
  assert.equal(typeof project.name, "string");
  console.log(`project: ${project.name} (dirty=${project.dirty}, saved=${project.saved})`);

  const audio = await callJson("ihub_audio");
  assert.equal(typeof audio.open, "boolean");
  assert.equal(typeof audio.sampleRate, "number");
  console.log(`audio: ${audio.deviceType} ${audio.deviceName} ${audio.sampleRate} Hz / ${audio.bufferSize}`);

  const midi = await callJson("ihub_midi");
  assert.ok(Array.isArray(midi.inputs));
  console.log(`midi: ${midi.inputCount} inputs`);

  const plugins = await callJson("ihub_plugins");
  assert.ok(Array.isArray(plugins.plugins));
  assert.equal(typeof plugins.count, "number");
  console.log(`plugins: ${plugins.count}/${plugins.total}`);

  const transportState = await callJson("ihub_transport");
  assert.equal(typeof transportState.bpm, "number");
  assert.equal(typeof transportState.playing, "boolean");
  console.log(`transport: ${transportState.bpm} BPM, playing=${transportState.playing}`);

  const meters = await callJson("ihub_meters", { timeoutMs: 5000 });
  assert.equal(meters.type, "meters");
  assert.ok(meters.sequence > 0);
  assert.ok(Array.isArray(meters.slots));
  console.log(`meters: sequence ${meters.sequence}, master peak ${meters.master.peak}`);

  for (const uri of resources) {
    const result = await client.readResource({ uri });
    const text = result.contents?.[0]?.text;
    assert.equal(typeof text, "string", `${uri} returns text`);
    JSON.parse(text);
  }
  console.log(`resources readable: ${resources.length}`);

  console.log("smoke: PASS");
} catch (error) {
  console.error("smoke: FAIL", error);
  process.exitCode = 1;
} finally {
  clearTimeout(timeout);
  await client.close().catch(() => {});
}

async function callJson(name, args = {}) {
  const result = await client.callTool({ name, arguments: args });
  assert.notEqual(result.isError, true, `${name} did not return an error`);
  const text = result.content?.find((block) => block.type === "text")?.text;
  assert.equal(typeof text, "string", `${name} returned text`);
  return JSON.parse(text);
}
