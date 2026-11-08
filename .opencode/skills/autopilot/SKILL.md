---
name: Autopilot
description: Run the Instrument Hub task autopilot. Picks Time Manager TODO tasks, one fresh session and branch per task, verifies with Playwright, merges to main on success, moves blockers to ON HOLD, and respects DeepSeek off-peak hours.
---

# Autopilot

Use this skill when the user says "run autopilot", "start autopilot", or asks about the autopilot queue.

## Start it

Run the launcher from the repository root:

```bash
scripts/autopilot-run.sh
```

It detaches the driver with `setsid`, writes the log to `.autopilot/autopilot.log`, and prints the driver PID. Report the PID and log path to the user. Do not do the task work yourself.

Useful variants:

```bash
scripts/autopilot-run.sh --dry-run          # list the queue and branch names, change nothing
scripts/autopilot-run.sh --once             # one task only
scripts/autopilot-run.sh --ignore-schedule  # explicit override for peak-hour testing
```

## What it does

- Reads the TODO tasks from the Time Manager list named `Instrument Hub`.
- Sorts by the `T` number at the start of the task title and picks the lowest.
- Marks the task `IN_PROGRESS`.
- Creates one branch per task from `main`, named `autopilot/<task-code>-<slug>`.
- Starts a fresh `opencode run` session with the task brief attached.
- The session researches, implements, builds the affected target, runs the app and verifies with Playwright.
- On success the driver commits the branch, merges it back into `main` with `--no-ff`, deletes the branch, appends an Autopilot section to the task description and marks it `DONE`.
- On a blocker or three failed attempts the driver checkpoints the branch, appends a reason to the description and moves the task to `ON_HOLD`, then continues with the next TODO task.
- It never pushes. It never starts a session during DeepSeek peak hours unless `--ignore-schedule` is passed.
- It stops only when there are no TODO tasks left. ON HOLD tasks are for the human.

## Inspect

```bash
tail -f .autopilot/autopilot.log
git branch --list 'autopilot/*'
```

Per-task output is in `.autopilot/logs/`. The driver lock is `.autopilot/autopilot.lock`.

## Stop

Kill the PID from `.autopilot/autopilot.lock` or `.autopilot/launcher.pid`. The current task remains `IN_PROGRESS` and its branch is left in place.

## Credentials

The driver reads the Time Manager MCP URL and bearer token from the `time-manager` entry in the OpenCode configuration. `IH_TM_MCP_URL` and `IH_TM_TOKEN` override it. Never write the token into the repository or logs.
