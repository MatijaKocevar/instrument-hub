# Instrument Hub — Autopilot Pack

Everything needed to run this project unattended.

| File | Purpose |
|---|---|
| `AUTOPILOT.md` | Operating contract: session protocol, blocker protocol, guardrails |
| `ROADMAP.md` | 55 tasks, dependency-ordered, with acceptance criteria |
| `DECISIONS.md` | Locked technical decisions — the agent must not re-litigate |
| `TESTING.md` | How to verify everything with no hardware attached |
| `BLOCKERS.md` | The human queue. Read this when you come back |
| `autopilot.py` | The driver |
| `STATE.json` | Ledger (generated) |

## Start

```bash
python3 autopilot.py --dry-run     # verify the queue parses
python3 autopilot.py               # run for real
```

`--max-sessions N` to bound a run. `--ignore-schedule` to skip the off-peak wait.
`--reset-failed` to retry blocked and partial tasks.

## How it behaves

- One `opencode run` per task → fresh context, never bloated.
- Waits out DeepSeek peak hours (01:00–04:00, 06:00–10:00 UTC, Mon–Fri).
- A blocker stops one task, not the run.
- Commits after every completed task.
- Stops when nothing runnable is left.

## Before the first run

`BLOCKERS.md` B001: the C++ headers need one `sudo apt install`.
No amount of automation gets past that.
