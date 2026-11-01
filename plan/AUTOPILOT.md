# Autopilot Contract

Operating rules for the unattended build of **Instrument Hub**.
Read this file first, every session.

- **Repo:** `/home/mkocevar/Projects/instrument-hub`
- **Driver:** `autopilot.py` (in this folder)
- **Task queue:** `ROADMAP.md`
- **Ledger:** `STATE.json` (owned by the driver, not by you)
- **Human queue:** `BLOCKERS.md` (you append to this, never remove)
- **Locked decisions:** `DECISIONS.md` (do not re-litigate)
- **Verification recipes:** `TESTING.md`

---

## 1. The one human step

`sudo apt install` of the C++ dev headers. Everything else is unattended.
If that has not happened, `T000` is blocked and so is the whole project.
The driver will log this and stop; it will not retry.

---

## 2. Session protocol

One task per `opencode run` invocation. Fresh context every time.
That is the whole point: no session ever grows past one task.

For each task the driver:

1. Extracts that task's block from `ROADMAP.md` into `.autopilot/<TASK>.md`
2. Deletes any stale `.autopilot/<TASK>.result`
3. Runs `opencode run --standalone --auto` with that brief
4. Reads `.autopilot/<TASK>.result`

Your job inside a session:

- Read the brief. Read `DECISIONS.md` if the brief references it.
- Do **only** that task. Do not start the next one.
- Do not ask questions. The human is not there.
- Run the acceptance check yourself before declaring done.
- Run tests with a hard timeout: `scripts/test.sh` or `ctest --timeout 120`.
  A hung test must fail within two minutes; never treat "still running" as
  progress.
- Write the result file, exactly in this format:

```
STATUS: done
SUMMARY: one or two lines on what you changed
EVIDENCE: command you ran and its key output
```

or

```
STATUS: blocked
REASON: what stopped you
NEEDED: precisely what a human must do
```

or

```
STATUS: partial
REASON: how far you got and what remains
```

Writing `done` when the acceptance check does not pass is the worst
possible outcome. Prefer `partial`.

### Retries

A session that dies before writing a result file (`rc != 0`, a transport
error, a timeout) has not judged the task. The driver backs off (1m, then
5m) and retries it, up to three attempts, then blocks it with a
`BLOCKERS.md` entry carrying the last error line.

Each retry's brief carries an `## Attempt history` section: the previous
session's edits are still in the working tree, so the session verifies and
finishes that work instead of starting the task over. Two sessions
rewriting the same files is how a task ends up looking like it runs forever.

Sessions run with `--standalone`. The shared background service is not
involved, so a dead CLI cannot leave a session editing the repo while the
driver retries the same task.

---

## 3. Blocker protocol

**A blocker never stops the run.** It stops *one task*.

When you hit a wall:

1. Append an entry to `BLOCKERS.md`:

```
## B0xx · <short title>
- task: T0xx
- severity: hard | soft
- what: what cannot be done
- why: root cause
- human must: exact steps the human performs
- workaround: what the run does instead, if anything
```

2. Write `STATUS: blocked` with `NEEDED:` pointing at that entry.
3. Stop. The driver moves to the next task with satisfied dependencies.

Never block on something you could stub behind an interface and test with a fake.
Prefer a fake device, a synthetic MIDI stream, or a recorded fixture over stopping.

---

## 4. Scheduling

DeepSeek peak hours are **01:00–04:00 and 06:00–10:00 UTC, Monday–Friday**.
Off-peak is half price. Weekends are entirely off-peak.

The driver waits out peak before starting a session, and refuses to *start*
a session within 20 minutes of a peak block. It never interrupts a session
that is already running — that would waste the tokens already spent.

Chinese public holidays are approximated as normal weekdays. Accept the
small error; do not try to compute them.

---

## 5. Guardrails

- Work only inside `REPO`. Never `sudo`, never touch `~/.ssh`, `~/.config`, or `/etc`.
- Never `git push`, never create remotes, never force-push.
- Never delete a directory you did not create in this run.
- If a task needs a new dependency, prefer a header-only or vendored one.
  Ask for `apt` only through `BLOCKERS.md`.
- Keep the audio callback free of allocation, locks, and logging. This is
  the one rule that will not be relaxed for convenience.
- Every commit is a checkpoint. The driver commits after each `done`.

---

## 6. Definition of done for the whole run

The run ends when every task is `done` or `blocked` and `BLOCKERS.md` holds
the remaining human work.

A successful end state:

- Engine builds and its tests pass on Linux
- Audio runs through the ALSA loopback with measured latency and zero xruns
- Plugins host, save, and reload
- Slots switch without clicks (proven by the discontinuity test)
- The Launchpad driver passes its scripted protocol, mapping and LED tests
- The React UI runs, is embedded, and is screenshotted
- `HANDOVER.md` lists the hardware checks a human still has to do

The run does **not** end at a milestone boundary. It ends at `BLOCKERS.md`.

---

## 7. Bug-fix phase

The feature roadmap is complete (`T000–T139`). Do not invent feature or
refactor tasks, and do not start a run "to make progress".

New work is added to `ROADMAP.md` one bug at a time from `plan/BUGS.md`,
after the bug has been reproduced. Each fix needs a regression test where
practical and must leave `scripts/test.sh` green. Keep one bug per commit.
