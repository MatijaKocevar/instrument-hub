# Running Instrument Hub on a new machine

The autopilot is not a machine — it is the driver, the ledger and the
protocols, and all of them live in this repository:

| Piece | File | Travels in git |
|---|---|---|
| Driver / scheduler / commits / off-peak logic | `plan/autopilot.py` | yes |
| Task queue and acceptance checks | `plan/ROADMAP.md` | yes |
| Ledger (what is done, what is next) | `plan/STATE.json` | yes |
| Session + blocker protocol | `plan/AUTOPILOT.md` | yes |
| Locked decisions, test recipes | `plan/DECISIONS.md`, `plan/TESTING.md` | yes |

So the flow is identical on every machine: one `opencode run` per task,
fresh context, run the acceptance check, commit the checkpoint, move on.
Nothing about that is local to the machine that started it.

What *is* local: the toolchain, the audio devices, the opencode
credentials, the git identity and the loopback modules. That is what
`scripts/bootstrap.sh` reproduces.

---

## Quick start

```bash
gh auth login
git clone --recursive git@github.com:MatijaKocevar/instrument-hub.git ~/Projects/instrument-hub
cd ~/Projects/instrument-hub
scripts/bootstrap.sh            # checks, builds, runs the full test suite
scripts/bootstrap.sh --install  # same, installing missing packages first
scripts/bootstrap.sh --start    # ... then starts the autopilot
```

`--install` runs the distro package manager and loads `snd-aloop` /
`snd-virmidi`, asking for your password in the terminal. Without the flag
the script only reports what is missing and refuses to build.

`scripts/bootstrap.sh --check` is diagnostics only and changes nothing.

## What the script sets up

- toolchain: `git cmake ninja pkg-config node npm python3 opencode`
- headers: `alsa jack freetype2 x11 gl webkit2gtk-4.1`
- audio/MIDI: `/dev/snd/seq`, an ALSA loopback card, realtime limits,
  `audio` group
- JUCE submodule (fetched if absent)
- `ui/node_modules` (`npm ci`)
- repo-local git identity — required, because the driver commits with
  `check=False` and would otherwise fail silently
- opencode: `run --auto`, DeepSeek credentials, model `deepseek/deepseek-flash`
  (override with `IH_MODEL`)
- build and `ctest`

## What it cannot do for you

1. **`opencode auth login`** — credentials do not travel between machines.
2. **Anything on a distro other than Ubuntu/Debian/Fedora/Arch** — the
   package lists in `--install` cover those four; elsewhere install the
   equivalents by hand.

The autopilot itself never runs `sudo`; `--install` is the human-supervised
path, and it is the only part of the setup that needs a password.

## Running

```bash
python3 plan/autopilot.py --dry-run    # confirm the queue and next task
python3 plan/autopilot.py              # run under tmux/screen
```

The driver derives the repo from its own location (`plan/autopilot.py`), so
a checkout anywhere works; `IH_REPO` overrides it if needed. Scheduling is
computed in UTC (peak: 01–04 and 06–10 UTC, Mon–Fri; weekends off-peak), so
the machine timezone does not matter. `--max-sessions N` bounds a run,
`--ignore-schedule` skips the wait.

## Stopping cleanly

`opencode run` executes the session inside the shared background service.
Killing the driver or the CLI does **not** stop it: the session keeps
editing files server-side, and the driver treats the dead CLI as `rc=1` and
retries the same task.

```bash
pkill -TERM -f "autopilot.py"
pkill -TERM -f "opencode run"

opencode api GET /api/session | python3 -c \
  "import json,sys;[print(s['id'],s['title']) for s in json.load(sys.stdin)['data'] if 'idle' not in s['time']]"
opencode api POST /api/session/<SESSION_ID>/interrupt
```

Then inspect `git status` for files the interrupted task left behind.

## Moving between machines

The autopilot only commits locally — it never pushes. GitHub is a manual
sync point:

```bash
# machine A, after stopping the driver
git push
# machine B
git pull
```

Never run two drivers against diverged clones; the ledger will fight.
