# Projects and app settings

Two stores, on purpose. A device name means nothing on another machine; a
set of instruments is the work.

| | App settings | Project |
| --- | --- | --- |
| file | `~/.config/Instrument Hub/settings.json` | one `.ihub` file |
| holds | audio device, enabled MIDI inputs, LAN mode, pairing token, app mixer (master, input gain, MIDI channel, metronome), last and recent projects | name, notes, 8 families / 64 pads, patch colours, sources, per-patch mixer (volume, pan, mute, solo), FX chains, per-patch plug-in state, mapping (scenes, macros, banks), transport |
| lifetime | this machine, this user | this body of work; movable between machines |
| writes | on an accepted change | debounced autosave + explicit save |

## Project file

`.ihub`, a zip, as `DECISIONS.md` requires:

```
manifest.json          schemaVersion, name, notes, transport
mapping.yaml           the raw YAML the mapping loader already parses
plugin-state/<id>.bin  one blob per patch that has a sound
```

`ihub::project::SetFile` already models exactly this and
`Serializer::save`/`load` already write and read it. The runtime just never
calls them.

## Where projects live

- `--project <path>` wins; otherwise the last opened project from app
  settings; otherwise `~/Music/Instrument Hub/Untitled.ihub`, created from
  the blank eight-track template `shared/tracks-set.yaml` on first launch.
- `shared/set.yaml` plus `shared/set.plugin-state/` are the optional Starter
  Set: **import-only**, converted into one `.ihub` when opened, and the app
  never writes YAML again.
- Recent projects (path + name) live in app settings, not in the project.

## Save rules

- An accepted edit updates memory and the audio graph immediately; the file
  is written after a 500 ms debounce, so a burst of fader moves is one write.
- Every save captures the running plug-in state for touched slots first.
  Today a sound is only captured on switch-away, editor close or eviction, so
  quitting right after an edit loses it.
- Write to `<name>.ihub.tmp` in the same directory, then rename; keep the
  previous file as `<name>.ihub.bak`. A failed write leaves the old file
  intact and returns the error on the request.
- A read-only or unwritable project keeps running, shows "not saved" in the
  footer, and never retries silently forever.
- Restoring app settings never writes anything.

## Startup order

1. load app settings
2. resolve the project path (`--project` > last > default)
3. load the project, apply it to the session
4. open the audio device: CLI flag > app settings > default

## Hot reload

The mapping watcher follows the project file's mtime instead of `set.yaml`'s;
an external replace re-routes pads within the existing 25 ms interval.

## Out of scope

Cloud sync, several projects open at once, undo history across saves.

## Tasks

| Task | Scope |
| --- | --- |
| T199 | Project container: `.ihub` as the live project, import `set.yaml` + sidecar once, load at startup. |
| T204 | Autosave: debounce, plug-in capture, dirty flag, atomic write, failure path. |
| T205 | Lifecycle: `--project`, default location, open/new/save-as, recent list. |
