import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

export async function checkReadOnlyMode({
  packageRoot,
  env,
  readTools,
  resources,
}) {
  const client = new Client({
    name: "ihub-mcp-smoke-read-only",
    version: "0.1.0",
  });
  const transport = new StdioClientTransport({
    command: process.execPath,
    args: ["dist/index.js"],
    cwd: packageRoot,
    env: { ...env, IHUB_MCP_READ_ONLY: "1" },
    stderr: "inherit",
  });

  try {
    await client.connect(transport);
    const { tools } = await client.listTools();
    assert.deepEqual(
      tools.map((tool) => tool.name).sort(),
      [...readTools].sort(),
      "read-only mode registers only read tools",
    );
    for (const tool of tools) {
      assert.equal(
        tool.annotations?.readOnlyHint,
        true,
        `${tool.name} stays read-only`,
      );
    }

    const { resources: listed } = await client.listResources();
    assert.deepEqual(
      listed.map((resource) => resource.uri).sort(),
      [...resources].sort(),
      "read-only mode keeps the resources",
    );

    const state = JSON.parse(await callText(client, "ihub_state"));
    assert.equal(
      state.pages?.[0]?.status,
      "ready",
      "read-only mode still reads live state",
    );

    const armError = await expectToolError(client, "ihub_arm_slot", {
      slot: 0,
    });
    assert.match(armError, /not found/i, "unregistered write tool is rejected");
    const projectError = await expectToolError(client, "ihub_project_action", {
      action: "save",
    });
    assert.match(
      projectError,
      /not found/i,
      "unregistered project tool is rejected",
    );

    console.log(
      `read-only: ${tools.length} read tools only, ${listed.length} resources, write calls rejected`,
    );
  } finally {
    await client.close().catch(() => {});
  }
}

export async function checkLoopbackGuard({ packageRoot, env }) {
  const remoteEnv = {
    ...env,
    IHUB_API_URL: "http://192.0.2.10:8080",
    IHUB_MCP_ALLOW_REMOTE: "",
  };
  const rejected = await waitForExit(packageRoot, remoteEnv, 5000);
  assert.notEqual(rejected.code, 0, "non-loopback IHUB_API_URL is refused");
  assert.match(
    rejected.stderr,
    /loopback/i,
    "refusal explains the loopback requirement",
  );
  console.log("security: non-loopback IHUB_API_URL refused without override");

  const allowed = spawn(process.execPath, ["dist/index.js"], {
    cwd: packageRoot,
    env: { ...remoteEnv, IHUB_MCP_ALLOW_REMOTE: "1" },
    stdio: ["pipe", "ignore", "pipe"],
  });
  let stderr = "";
  allowed.stderr.on("data", (chunk) => {
    stderr += chunk.toString();
  });
  await delay(1000);
  assert.equal(
    allowed.exitCode,
    null,
    `IHUB_MCP_ALLOW_REMOTE=1 starts the server (${stderr.trim()})`,
  );
  await stop(allowed);
  console.log("security: IHUB_MCP_ALLOW_REMOTE=1 permits the override");
}

export async function checkWriteGuards({ packageRoot, env }) {
  const client = new Client({
    name: "ihub-mcp-smoke-guards",
    version: "0.1.0",
  });
  const transport = new StdioClientTransport({
    command: process.execPath,
    args: ["dist/index.js"],
    cwd: packageRoot,
    env: {
      ...env,
      IHUB_API_URL: "http://127.0.0.1:9",
      IHUB_MCP_READ_ONLY: "",
      IHUB_MCP_ALLOW_REMOTE: "",
    },
    stderr: "inherit",
  });

  try {
    await client.connect(transport);
    const { tools } = await client.listTools();
    assert.ok(
      tools.some((tool) => tool.name === "ihub_clear_source"),
      "write tools are registered when not read-only",
    );

    const clearError = await expectToolError(client, "ihub_clear_source", {
      slot: 0,
    });
    assert.match(clearError, /confirm: true/i, "clear source needs confirmation");
    const projectError = await expectToolError(client, "ihub_project_action", {
      action: "open",
      path: "/tmp/guard-check.ihub",
    });
    assert.match(
      projectError,
      /confirm: true/i,
      "project open needs confirmation",
    );
    console.log("security: clear-source and project-open require confirm: true");
  } finally {
    await client.close().catch(() => {});
  }
}

async function callText(client, name) {
  const result = await client.callTool({ name, arguments: {} });
  assert.notEqual(result.isError, true, `${name} did not return an error`);
  const text = result.content?.find((block) => block.type === "text")?.text;
  assert.equal(typeof text, "string", `${name} returned text`);
  return text;
}

async function expectToolError(client, name, args) {
  let result;
  try {
    result = await client.callTool({ name, arguments: args });
  } catch (error) {
    return error instanceof Error ? error.message : String(error);
  }
  assert.equal(result.isError, true, `${name} was rejected`);
  return result.content?.find((block) => block.type === "text")?.text ?? "";
}

function waitForExit(packageRoot, env, timeoutMs) {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, ["dist/index.js"], {
      cwd: packageRoot,
      env,
      stdio: ["ignore", "ignore", "pipe"],
    });
    let stderr = "";
    const timer = setTimeout(() => {
      child.kill("SIGKILL");
      reject(new Error("loopback guard did not exit in time"));
    }, timeoutMs);
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString();
    });
    child.on("error", (error) => {
      clearTimeout(timer);
      reject(error);
    });
    child.on("close", (code) => {
      clearTimeout(timer);
      resolve({ code, stderr });
    });
  });
}

function stop(child) {
  return new Promise((resolve) => {
    child.once("close", resolve);
    child.kill("SIGKILL");
  });
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
