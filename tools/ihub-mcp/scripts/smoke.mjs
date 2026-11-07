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
const controlTools = [
  "ihub_arm_track",
  "ihub_arm_slot",
  "ihub_set_track_volume",
  "ihub_toggle_track_fx",
  "ihub_set_transport",
  "ihub_set_midi_input",
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
  assert.deepEqual(
    toolNames,
    [...readTools, ...controlTools].sort(),
    "read and control tools registered",
  );
  const toolsByName = new Map(tools.map((tool) => [tool.name, tool]));
  for (const name of readTools) {
    assert.equal(
      toolsByName.get(name)?.annotations?.readOnlyHint,
      true,
      `${name} is annotated read-only`,
    );
  }
  for (const name of controlTools) {
    assert.equal(
      toolsByName.get(name)?.annotations?.readOnlyHint,
      false,
      `${name} is annotated state-changing`,
    );
  }
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

  const before = await callJson("ihub_state");
  assert.ok(Array.isArray(before.tracks) && before.tracks.length > 2, "state has track 2");
  const originalSelected = before.tracks.find((track) => track.selected);
  const originalStep = before.tracks[2].volumeStep;

  const armed = await callJson("ihub_arm_track", { index: 2 });
  assert.equal(armed.armedSlot, armed.tracks[2].slot, "arm response has track 2 slot");
  const armedState = await callJson("ihub_state");
  assert.equal(armedState.tracks[2].selected, true, "state confirms track 2 selected");
  assert.equal(armedState.armedSlot, armedState.tracks[2].slot, "state confirms track 2 armed");
  console.log(`armed track index 2, slot ${armedState.armedSlot}`);

  const slotArmed = await callJson("ihub_arm_slot", { slot: armedState.armedSlot });
  assert.equal(slotArmed.armedSlot, armedState.armedSlot, "arm slot confirms the slot");
  console.log(`arm slot: ${slotArmed.armedSlot}`);

  const newStep = originalStep === 4 ? 3 : 4;
  const volume = await callJson("ihub_set_track_volume", { index: 2, step: newStep });
  assert.equal(volume.tracks[2].volumeStep, newStep, "volume response has the new step");
  const volumeState = await callJson("ihub_state");
  assert.equal(volumeState.tracks[2].volumeStep, newStep, "state confirms the volume step");
  console.log(`track index 2 volume: step ${originalStep} -> ${newStep}`);

  const originalTransport = await callJson("ihub_transport");
  const newBpm = originalTransport.bpm === 128 ? 120 : 128;
  const updatedTransport = await callJson("ihub_set_transport", { bpm: newBpm });
  assert.equal(updatedTransport.bpm, newBpm, "transport response has the new BPM");
  const transportAfter = await callJson("ihub_transport");
  assert.equal(transportAfter.bpm, newBpm, "transport confirms the new BPM");
  console.log(`transport: ${originalTransport.bpm} -> ${transportAfter.bpm} BPM`);

  const midiBefore = await callJson("ihub_midi");
  const midiTarget =
    midiBefore.inputs.find((input) => input.role !== "launchpad" && !input.enabled) ??
    midiBefore.inputs.find((input) => input.role !== "launchpad");
  assert.ok(midiTarget, "a non-Launchpad MIDI input is available");
  const originalEnabled = midiTarget.enabled;

  if (originalEnabled) {
    const disabled = await callJson("ihub_set_midi_input", {
      id: midiTarget.id,
      enabled: false,
    });
    assert.equal(findInput(disabled, midiTarget.id)?.enabled, false, "MIDI input disabled");
    const disabledState = await callJson("ihub_midi");
    assert.equal(findInput(disabledState, midiTarget.id)?.enabled, false, "midi confirms disabled");
    console.log(`midi ${midiTarget.name}: enabled -> disabled`);
  }

  const enabled = await callJson("ihub_set_midi_input", {
    id: midiTarget.id,
    enabled: true,
  });
  assert.equal(findInput(enabled, midiTarget.id)?.enabled, true, "MIDI input enabled");
  const enabledState = await callJson("ihub_midi");
  assert.equal(findInput(enabledState, midiTarget.id)?.enabled, true, "midi confirms enabled");
  console.log(`midi ${midiTarget.name}: disabled -> enabled`);

  if (!originalEnabled) {
    const restored = await callJson("ihub_set_midi_input", {
      id: midiTarget.id,
      enabled: false,
    });
    assert.equal(findInput(restored, midiTarget.id)?.enabled, false, "MIDI input restored");
    const restoredState = await callJson("ihub_midi");
    assert.equal(findInput(restoredState, midiTarget.id)?.enabled, false, "midi confirms restore");
    console.log(`midi ${midiTarget.name}: restored to disabled`);
  }

  await callJson("ihub_set_track_volume", { index: 2, step: originalStep });
  await callJson("ihub_set_transport", { bpm: originalTransport.bpm });
  if (originalSelected) {
    const restoredArm = await callJson("ihub_arm_track", {
      index: originalSelected.index,
    });
    assert.equal(restoredArm.armedSlot, originalSelected.slot, "original track re-armed");
    console.log(`restored arm: track index ${originalSelected.index}`);
  } else {
    const restoredArm = await callJson("ihub_arm_slot", { slot: null });
    assert.equal(restoredArm.armedSlot, -1, "slots disarmed");
    console.log("restored arm: disarmed");
  }
  const finalState = await callJson("ihub_state");
  assert.equal(finalState.tracks[2].volumeStep, originalStep, "state confirms volume restore");
  const finalTransport = await callJson("ihub_transport");
  assert.equal(finalTransport.bpm, originalTransport.bpm, "transport confirms BPM restore");
  console.log("control tools: PASS");

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

function findInput(midiState, id) {
  return midiState.inputs.find((input) => input.id === id);
}
