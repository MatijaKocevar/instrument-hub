# Running Instrument Hub on a new machine

Setup runbook for a fresh Linux box.

The autopilot agent needs nothing from this file: it reads `AGENTS.md` and
`plan/AUTOPILOT.md` at the start of every session, and the driver writes a
self-contained brief per task. This is the **human** work, done once per
machine.

Resume state lives in `plan/STATE.json` (the ledger) and `plan/BLOCKERS.md`
(your queue). Read those, not this file, to find out where the build stands.

---

## 1. Clone

```bash
mkdir -p ~/Projects
git clone git@github.com:MatijaKocevar/instrument-hub.git ~/Projects/instrument-hub
cd ~/Projects/instrument-hub
git submodule update --init --recursive      # JUCE 8, needs github.com
```

## 2. Toolchain (sudo, once)

Same set as `BLOCKERS.md` B001:

```bash
sudo apt install build-essential pkg-config libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev \
  libglu1-mesa-dev libcurl4-openssl-dev ninja-build cmake nodejs npm python3 git
```

Audio and MIDI tests assume the ALSA loopback and virtual MIDI:

```bash
aplay -l | grep -i loopback    # TESTING.md expects a Loopback card
sudo modprobe snd-aloop        # if it is missing
```

## 3. UI dependencies

```bash
npm --prefix ui ci
```

## 4. Git identity — required

The driver commits with `check=False`. Without an identity, every checkpoint
silently fails to be created and the run looks like it worked.

```bash
git config user.name  "Instrument Hub Autopilot"
git config user.email "autopilot@instrument-hub.local"
```

## 5. opencode

- Install opencode v2.x on `PATH`.
- `opencode run --help` must list `--auto` (the driver uses
  `run --auto --agent build -m <model> --file <brief>`).
- Authenticate DeepSeek (`opencode auth login`); credentials do not travel.
- `opencode models | grep deepseek` must resolve `deepseek/deepseek-flash`,
  otherwise export `IH_MODEL=<provider/model>`.
- Agent `build` is built in; no agent file to copy.
- The local `opencode.jsonc` permission block must not *deny* tools the run
  needs — `--auto` only approves what is not denied.

## 6. Repo path

`plan/autopilot.py` defaults to `/home/mkocevar/Projects/instrument-hub`.
Elsewhere:

```bash
export IH_REPO="$HOME/Projects/instrument-hub"    # put it in ~/.zshrc
```

Briefs and `--file` paths are generated from `IH_REPO`, so stale paths in
the docs are cosmetic.

## 7. Prove the checkout

```bash
scripts/build.sh
ctest --test-dir build --output-on-failure
```

## 8. Run

```bash
python3 plan/autopilot.py --dry-run    # confirm the queue and next task
python3 plan/autopilot.py              # run under tmux/screen
```

Scheduling is computed in UTC (peak: 01–04 and 06–10 UTC, Mon–Fri;
weekends are off-peak), so the machine timezone does not matter.
`--max-sessions N` bounds a run, `--ignore-schedule` skips the wait.

## 9. Stop cleanly

`opencode run` executes the session inside the shared background service.
Killing the driver or the CLI does **not** stop it: the session keeps
editing files server-side, and the driver treats the dead CLI as `rc=1` and
retries the same task.

```bash
pkill -TERM -f "autopilot.py"
pkill -TERM -f "opencode run"

# interrupt anything still busy
opencode api GET /api/session | python3 -c \
  "import json,sys;[print(s['id'],s['title']) for s in json.load(sys.stdin)['data'] if 'idle' not in s['time']]"
opencode api POST /api/session/<SESSION_ID>/interrupt
```

Then inspect `git status` for files the interrupted task left behind.

## 10. Moving between machines

The autopilot only commits locally — it never pushes. GitHub is a manual
sync point:

```bash
# machine A, after stopping the driver
git push
# machine B
git pull
```

Never run two drivers against diverged clones; the ledger will fight.
