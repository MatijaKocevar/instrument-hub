# Instrument Hub MCP adapter

Standalone Model Context Protocol server for the Instrument Hub control API.
It speaks MCP over stdio and exposes read-only tools, control tools and
resources backed by the engine's HTTP and WebSocket endpoints.

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
engine: initialization, the read-only and write tool lists, the resource
list, state with `pages[0].status: "ready"`, project, audio, MIDI, plugin
inventory, transport, one meter frame and all five resources. It then arms
track 2, sets its volume, changes the transport BPM, toggles a virtual MIDI
input and confirms every change through `/state`, `/transport` and `/midi`,
restoring the original values at the end. It captures plugin state on slot 0,
saves the open project, and rejects a bundle-path string passed to
`ihub_assign_plugin`.

The smoke client also starts the server with `IHUB_MCP_READ_ONLY=1` and
confirms that only the eight read tools are registered and that write calls
fail. It verifies the loopback guard and the `confirm: true` gates for clearing
a source and for opening a project. It deliberately does not call
`ihub_restart_page`: a page recovery can take up to 180 seconds and the first
attempt can stall (BUG-029).

The engine must be running with its API listener enabled. The server writes
protocol messages to stdout, so diagnostics go to stderr. `node dist/index.js`
is normally launched by the MCP client, not by hand.

## Configuration

| Variable               | Default                 | Meaning                                                                 |
| ---------------------- | ----------------------- | ----------------------------------------------------------------------- |
| `IHUB_API_URL`         | `http://127.0.0.1:8080` | Engine HTTP base URL. `https` upgrades to `wss`.                        |
| `IHUB_TOKEN`           | unset                   | Pairing token sent as `X-Instrument-Hub-Token`.                         |
| `IHUB_MCP_READ_ONLY`   | unset                   | `1`/`true` registers only read tools and resources; write tools are absent. |
| `IHUB_MCP_ALLOW_REMOTE`| unset                   | `1`/`true` allows a non-loopback `IHUB_API_URL`.                        |

Auth is only enforced when the engine was started with a pairing token.
Without `IHUB_MCP_ALLOW_REMOTE`, the server refuses to start when
`IHUB_API_URL` is not `127.0.0.0/8`, `::1` or `localhost`.

## Tools

Read tools are annotated `readOnlyHint: true` and `destructiveHint: false`.

| Tool                        | Reads                                                    |
| --------------------------- | -------------------------------------------------------- |
| `ihub_state`                | `GET /state` — slots, tracks, pages, settings, transport |
| `ihub_project`              | `GET /project` — open project and recent list            |
| `ihub_audio`                | `GET /audio` — device, latency, channels, xruns          |
| `ihub_midi`                 | `GET /midi` — inputs, roles, pad test                    |
| `ihub_plugins`              | `GET /plugins` — inventory, optional `search`            |
| `ihub_meters`               | One frame from the `/meters` WebSocket                   |
| `ihub_transport`            | `GET /transport` — playing, BPM, PPQ, samples            |
| `ihub_wait_for_page_ready`  | Polls `/state` until a page status is `ready`            |

Write tools are annotated `readOnlyHint: false`. Tools that can discard
unsaved state or replace a source are also annotated `destructiveHint: true`.

| Tool                        | Endpoint                                                | Notes                                                      |
| --------------------------- | ------------------------------------------------------- | ---------------------------------------------------------- |
| `ihub_arm_track`            | `POST /tracks/{index}/select`                           | Arm a track's slot.                                        |
| `ihub_arm_slot`             | `POST /arm`                                             | Arm a slot, or disarm with `null`/`-1`.                    |
| `ihub_set_track_volume`     | `POST /tracks/{index}/volume`                           | Stepped volume 0-4.                                        |
| `ihub_toggle_track_fx`      | `POST /tracks/{index}/fx/{hot}/toggle`                  | Hot FX bypass.                                             |
| `ihub_set_transport`        | `POST /transport`                                       | Playing, BPM, PPQ or sample position.                      |
| `ihub_set_midi_input`       | `POST /midi/inputs`                                     | Enable, role, note base or pad learn.                      |
| `ihub_assign_plugin`        | `GET /plugins`, then `POST /patches/{slot}/source`      | Resolves a plugin id or exact name; destructive.           |
| `ihub_clear_source`         | `POST /patches/{slot}/source`                           | Sends `{kind: "empty"}`; needs `confirm: true`.            |
| `ihub_open_editor`          | `POST /patches/{slot}/editor`                           | Patch must host a resident plugin.                         |
| `ihub_close_editor`         | `DELETE /patches/{slot}/editor`                         | Idempotent.                                                |
| `ihub_capture_patch_state`  | `POST /patches/{slot}/state/capture`                    | Saves the running plugin's sound into the patch.           |
| `ihub_reload_plugin`        | `POST /patches/{slot}/reload`                           | Restarts from committed state; destructive.                |
| `ihub_restart_page`         | `POST /pages/{page}/restart`                            | Restarts the shared page host; destructive, see BUG-029.   |
| `ihub_project_action`       | `POST /project`                                         | `new`, `open`, `save`, `save-as`; destructive.             |

`ihub_assign_plugin` accepts a plugin id from `ihub_plugins`, or an exact
display name plus an optional exact `vendor`. It never accepts a bundle
path: the path sent to the engine always comes from the `/plugins` inventory.

`ihub_clear_source` and the `new`/`open` actions of `ihub_project_action`
require `confirm: true`. `open` and `save-as` require an absolute `.ihub`
path. `save` writes the open project and rejects a path.

`ihub_plugins` accepts an optional case-insensitive `search` argument.
`ihub_meters` and `ihub_wait_for_page_ready` accept an optional `timeoutMs`
argument. `ihub_set_transport` requires at least one field;
`ihub_set_midi_input` requires `id` plus at least one change. Track tools
return the state snapshot, transport returns the transport, MIDI control
returns the MIDI state, editor tools return the editor state, capture returns
the stored size, and the project tools return the project overview.

## Resources

| URI             | Content                     |
| --------------- | --------------------------- |
| `ihub://state`  | Complete state document     |
| `ihub://project`| Project overview            |
| `ihub://audio`  | Audio state                 |
| `ihub://midi`   | MIDI state                  |
| `ihub://plugins`| Plugin inventory            |

Resources stay available in read-only mode.

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
For an agent that should only inspect the engine, add
`"IHUB_MCP_READ_ONLY": "1"`; the write tools are then not registered.

## OpenCode

Add this to `opencode.json` or `~/.config/opencode/opencode.json` (use an
absolute path to `dist/index.js`). This example is read-only:

```json
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "instrument-hub": {
      "type": "local",
      "command": ["node", "/home/mkocevar/Projects/instrument-hub/tools/ihub-mcp/dist/index.js"],
      "enabled": true,
      "environment": {
        "IHUB_API_URL": "http://127.0.0.1:8080",
        "IHUB_MCP_READ_ONLY": "1"
      }
    }
  }
}
```

Remove `IHUB_MCP_READ_ONLY` to enable the control, plugin, editor, page and
project tools. Add `IHUB_TOKEN` to `environment` when the engine requires one.

A remote engine is refused by default. Only when it is intentionally exposed,
and preferably over TLS with a pairing token:

```bash
IHUB_API_URL=https://studio.example:8080 \
IHUB_TOKEN=<pairing-token> \
IHUB_MCP_ALLOW_REMOTE=1 \
node dist/index.js
```

## Development

`src/api/schema.d.ts` is generated from `shared/api/openapi.yaml` with
`openapi-typescript`; `pnpm build` regenerates it before compiling. The
runtime surface is split into small modules:

- `src/config.ts` — environment parsing and WebSocket URL conversion
- `src/security.ts` — read-only mode, loopback guard, confirmation gate
- `src/api/client.ts` — typed HTTP client composition (`openapi-fetch`)
- `src/api/patches.ts`, `src/api/project.ts` — patch and project endpoints
- `src/api/result.ts` — response unwrapping and API errors
- `src/api/meters.ts` — one-frame WebSocket reader
- `src/plugin-catalog.ts` — plugin identity and source resolution
- `src/tools/read.ts`, `src/tools/wait.ts` — MCP read tools
- `src/tools/tracks.ts` — track and slot control tools
- `src/tools/transport.ts`, `src/tools/midi.ts` — transport and MIDI tools
- `src/tools/plugin-source.ts` — plugin assignment and source clearing
- `src/tools/patch.ts` — editor, capture and plugin reload tools
- `src/tools/pages.ts` — shared page restart
- `src/tools/project.ts` — project lifecycle actions
- `src/tools/annotations.ts` — shared MCP tool annotations
- `src/resources.ts` — MCP resources
- `src/format.ts` — JSON/error result formatting
- `scripts/smoke.mjs` — scripted stdio smoke client
- `scripts/smoke-security.mjs` — read-only, confirmation and endpoint guards

## Security

- The adapter exposes only the tools and resources listed above. There is no
  generic HTTP proxy, no shell tool and no arbitrary endpoint parameter.
- `IHUB_API_URL` is loopback-only unless `IHUB_MCP_ALLOW_REMOTE=1` is set.
  Keep a remote engine behind TLS and a pairing token.
- Write tools are not registered at all when `IHUB_MCP_READ_ONLY=1`; a client
  calling one gets a tool-not-found error from the MCP server.
- Plugin assignment is resolved against the live `/plugins` inventory by id or
  exact name. A caller cannot inject a bundle path or plugin descriptor.
- Project switching (`new`, `open`) and clearing a source require
  `confirm: true`. Every other write is still annotated for the client.
- The destructive tools are `ihub_assign_plugin`, `ihub_clear_source`,
  `ihub_reload_plugin`, `ihub_restart_page` and `ihub_project_action`.
- `ihub_restart_page` can take minutes and may stall on the first recovery
  attempt (BUG-029). Do not call it from a timed loop or a health check.
