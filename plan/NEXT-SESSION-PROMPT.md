# Next session prompt

Repo: `/home/mkocevar/Projects/instrument-hub`

Read `AGENTS.md`, `plan/NEXT-SESSION.md`, `plan/BUGS.md` and
`plan/T313-SHARED-PLUGIN-HOST.md` first. This is an interactive session, not
the autopilot driver. Do not edit `plan/STATE.json`. Do not start the app or a
dev server yourself; use the developer's running instance, or ask them to
start one. Ask before builds.

## Implement T313 — shared plugin host per instrument page

Problem: each plugin slot currently gets its own `ihub-sandbox` + Wine host.
Eight warmed Analog Lab V tracks mean eight Wine processes; the eighth can
deadlock in `applyState` and the watchdog kills it, leaving Track 8 silent
(BUG-024). The same design multiplies RAM. The developer accepts that a shared
host failure takes the whole page down, and wants automatic recovery instead of
per-slot isolation.

Goal: one shared `ihub-sandbox`/Wine process per instrument page, with every
instrument on the page warmed and instant gain-ramp switching. If the host
dies or hangs, mute the page, restart the host, restore every slot's committed
plugin state, re-arm the active track and fade back in without crashing or
blocking the audio thread. Design for N slots and a page descriptor so future
pages of instruments can switch as a group; do not hard-code eight.

Only one armed track is audible at a time; do not turn this into a multitrack
mixer. Keep the fixed audio graph and the audio-thread contract unchanged.
Keep the old per-slot path behind a migration flag until the new path is
proven. Do not re-enable Wine plugin editors (BUG-019).

Start by reading the current sandbox path and proposing the smallest protocol
change, then use targeted builds. Acceptance criteria and the suggested
sequence are in `plan/T313-SHARED-PLUGIN-HOST.md`.

When T313 is done, stop and report the memory comparison, the crash-recovery
result and any remaining risks.
