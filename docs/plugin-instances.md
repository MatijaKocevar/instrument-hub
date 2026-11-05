# Plugin instances and editors

Every plugin slot on an instrument page is a `PageHostSlot` backed by one
`PageHost` and one `ihub-sandbox` process. The page host keeps a vector of slot
channels (`SharedMemory slots[]` in `SharedPage`); each channel has its own
request/reply rings, control command, heartbeat, editor state and committed
state. The slot index plus the monotonic generation recorded by `Session` still
guard state callbacks, so a retired slot cannot publish into a newer one. The
legacy one-`SandboxHost`-per-slot path is available with
`IHUB_SHARED_PLUGIN_HOST=0`.

## Editors

Each slot publishes its plugin state when its native editor window closes. The
editor runs in the page helper on the message thread; it writes the blob to a
per-slot file, stores the size and bumps a per-slot revision in shared memory,
then clears `editorOpen`. The page watchdog reads the revision, commits the
blob as that slot's last committed state, and notifies the session. The session
stores it in `PluginStateStore` and bumps the API state revision, so `/state`
observes both `editorOpen=false` and the new state without a close API call.

The helper keeps each native editor object alive after a close and parks its
window off-screen until the slot is unloaded or the editor is reopened. JUCE's
VST3 host embeds the plug-in view through `XEmbedComponent`; destroying that
view immediately after the user closes the window leaves queued X11 events
pointing at a destroyed component and can abort the helper. Reusing the same
window until unload keeps the XEmbed client valid.

## Reload and recovery

`POST /patches/{slot}/reload` and `POST /pages/{page}/restart` both use the
page recovery path:

1. Best-effort capture every healthy slot's live state.
2. Set the page to `recovering`; every slot source returns silence.
3. Kill the shared helper and its whole Wine process tree.
4. Start a new helper with the same page descriptor.
5. Reapply every slot's last committed state.
6. Re-arm the slot that was active before the failure and return to `ready`.

The automatic watchdog uses the same path with three bounded attempts and
backoff. If the page cannot recover it stays `failed`; the engine keeps running
and the track grid shows the page error. A manual restart resets the attempt
budget.

## Failure scope

A shared host failure is page-wide by design. `GET /state.pages[]` reports
`ready`, `recovering` or `failed`, the affected slot indices, the active slot
and the page error. The track grid shows one “Recovering instruments…” banner
and keeps the selected track highlighted instead of showing one failure per
slot. The state API still exposes a slot-local error when a single slot is
disabled or a legacy per-slot helper fails.
