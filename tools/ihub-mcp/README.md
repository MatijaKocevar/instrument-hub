# Instrument Hub MCP adapter

Standalone Model Context Protocol server for the Instrument Hub control API.
It speaks MCP over stdio and exposes read-only tools and resources backed by
the engine's HTTP and WebSocket endpoints.

This package does not link engine code. Start the Instrument Hub engine with
its API listener first (default `http://127.0.0.1:8080`), then point an MCP
client at `dist/index.js`.

## Requirements

- Node.js 22 or newer
- pnpm 11 (the repository pins it through `packageManager`; `corepack pnpm`
  works when pnpm is not installed globally)

## Build and run

```bash
cd tools/ihub-mcp
corepack pnpm install --frozen-lockfile
corepack pnpm typecheck
corepack pnpm build
corepack pnpm smoke
node dist/index.js
```

`corepack pnpm smoke` starts the built server over stdio and checks the live
engine: initialization, the read-only tool and resource lists, state with
`pages[0].status: "ready"`, project, audio, MIDI, plugin inventory,
transport, one meter frame and all five resources. The engine must be running
with its API listener enabled.

The server writes protocol messages to stdout, so diagnostics go to stderr.
`node dist/index.js` is normally launched by the MCP client, not by hand.

## Configuration

| Variable       | Default                 | Meaning                                             |
| -------------- | ----------------------- | --------------------------------------------------- |
| `IHUB_API_URL` | `http://127.0.0.1:8080` | Engine HTTP base URL. `https` upgrades to `wss`.     |
| `IHUB_TOKEN`   | unset                   | Pairing token sent as `X-Instrument-Hub-Token`.     |

Auth is only enforced when the engine was started with a pairing token.

## Tools

All tools are annotated `readOnlyHint: true`; none mutate engine state.

| Tool                        | Reads                                                |
| --------------------------- | ---------------------------------------------------- |
| `ihub_state`                | `GET /state` — slots, tracks, pages, settings, transport |
| `ihub_project`              | `GET /project` — open project and recent list        |
| `ihub_audio`                | `GET /audio` — device, latency, channels, xruns      |
| `ihub_midi`                 | `GET /midi` — inputs, roles, pad test                |
| `ihub_plugins`              | `GET /plugins` — inventory, optional `search`        |
| `ihub_meters`               | One frame from the `/meters` WebSocket               |
| `ihub_transport`            | `GET /transport` — playing, BPM, PPQ, samples        |
| `ihub_wait_for_page_ready`  | Polls `/state` until a page status is `ready`        |

`ihub_plugins` accepts an optional case-insensitive `search` argument.
`ihub_meters` and `ihub_wait_for_page_ready` accept an optional `timeoutMs`
argument. The wait tool defaults to the first page and accepts a `page` id.

## Resources

| URI             | Content                     |
| --------------- | --------------------------- |
| `ihub://state`  | Complete state document     |
| `ihub://project`| Project overview            |
| `ihub://audio`  | Audio state                 |
| `ihub://midi`   | MIDI state                  |
| `ihub://plugins`| Plugin inventory            |

## Claude Desktop

Add this to `claude_desktop_config.json` (use an absolute path):

```json
{
  "mcpServers": {
    "instrument-hub": {
      "command": "node",
      "args": ["/home/mkocevar/Projects/instrument-hub/tools/ihub-mcp/dist/index.js"],
      "env": {
        "IHUB_API_URL": "http://127.0.0.1:8080"
      }
    }
  }
}
```

Add `"IHUB_TOKEN": "<pairing token>"` to `env` when the engine requires one.

## OpenCode

Add this to `opencode.json` or `~/.config/opencode/opencode.json` (use an
absolute path to `dist/index.js`):

```json
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "instrument-hub": {
      "type": "local",
      "command": ["node", "/home/mkocevar/Projects/instrument-hub/tools/ihub-mcp/dist/index.js"],
      "enabled": true,
      "environment": {
        "IHUB_API_URL": "http://127.0.0.1:8080"
      }
    }
  }
}
```

Add `IHUB_TOKEN` to `environment` when the engine requires one.

## Development

`src/api/schema.d.ts` is generated from `shared/api/openapi.yaml` with
`openapi-typescript`; `pnpm build` regenerates it before compiling. The
runtime API surface is split into small modules:

- `src/config.ts` — environment parsing and WebSocket URL conversion
- `src/api/client.ts` — typed HTTP client (`openapi-fetch`)
- `src/api/meters.ts` — one-frame WebSocket reader
- `src/tools/read.ts`, `src/tools/wait.ts` — MCP read tools
- `src/resources.ts` — MCP resources
- `src/format.ts` — JSON/error result formatting
- `scripts/smoke.mjs` — scripted stdio smoke client

## Security

The adapter only registers read-only tools. It does not expose arbitrary HTTP
passthrough, plugin paths, project writes, transport control or track control.
Keep `IHUB_API_URL` on loopback unless you intentionally expose the engine and
configure a pairing token.
