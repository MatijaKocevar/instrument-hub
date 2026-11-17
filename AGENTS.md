# AGENTS.md

Operating rules for any agent working in this repository.

Instrument Hub: a JUCE 8 C++20 audio engine with an embedded React UI,
driven by a Novation Launchpad Mini MK3 and controlled over a local HTTP +
WebSocket API, with an optional HTTPS/WSS phone MIDI bridge over the LAN.

---

## Working with the developer

The human on this machine is a developer working interactively. They expect
changes to be built and the running app restarted so they can see them.

- After a change, build the affected target and restart the running app
  (engine, Vite, or both) without asking. Reuse the command line of the
  running instance, launch detached (desktop entry / `gtk-launch`, or
  `setsid` from a long-lived terminal), and leave it running for the
  developer.
- Never run a build without an explicit target. `cmake --build <dir>`,
  bare `ninja`, and `scripts/build.sh` build the whole tree. Always pass
  `--target`.
- Testing is disabled. Do not create, add, build, or run tests, test targets,
  test runners, snapshot harnesses, or test tooling. Do not add test files,
  test dependencies, or test instructions. Verify only by building the app
  and using it.
- Do not build after every edit just for the sake of it. Build when the
  change is ready to run, then restart the app once.

## Phone MIDI bridge

A paired Android phone can read a USB-MIDI keyboard with Chrome's Web MIDI
and forward the notes to the engine over WiFi. The desktop sees a normal MIDI
input named `Phone MIDI` with port `WiFi` and id `net:<client>`.

How it is wired:

- The desktop control API stays HTTP on `127.0.0.1:8080`.
- LAN mode starts a second HTTPS/WSS listener on `0.0.0.0:8443`, using a
  generated self-signed certificate in `~/.config/Instrument Hub/tls`.
- Never rebind or stop the desktop HTTP listener when toggling LAN mode. That
  was the bug that took the whole API down and left the phone with a dead
  QR. Toggling LAN only starts/stops the HTTPS listener and discovery.
- `wss://<lan-ip>:8443/midi-input?token=...` carries tagged binary MIDI
  frames (`0x01` + raw MIDI bytes). Each connection becomes a virtual keyboard
  input; held notes are released when the socket closes.
- The bridge controller lives in
  `ui/src/features/remote/RemoteRoute.tsx`, above the stage/MIDI view switch.
  It keeps the WebSocket and the MIDI input listener alive while the user
  changes tracks. The stage header shows **MIDI ON**.
- `AuthGuard` trusts loopback requests. The desktop UI therefore keeps
  working with LAN mode on and after restarts; LAN clients still need a
  pairing token.
- Only the last pairing token is persisted in settings. After an app restart
  the phone may need to reload and rescan the QR once; the desktop does not
  need to re-pair.

Phone flow:

1. Desktop **Settings & Account** → **Turn on LAN mode**. Keep the app open.
2. On Android Chrome (not Safari), open the QR URL:
   `https://<lan-ip>:8443/remote?code=...`. Accept the self-signed certificate
   warning once.
3. Pair, connect the keyboard to the phone with a USB OTG adapter, open
   **MIDI**, allow MIDI devices, select the keyboard, tap
   **Turn on WiFi MIDI**.
4. Tap **← Stage**. The header reads **MIDI ON**; switch tracks freely.
5. Turn it off from the MIDI page to disconnect and release held notes.

Constraints: Web MIDI needs a secure context, so the phone must use the HTTPS
URL, never `http://...:8080`. Android Chrome is required; iPhone Safari has no
Web MIDI. The keyboard must be USB-MIDI class compliant; 5-pin DIN needs a
USB-MIDI interface.

## Commit messages

Use Conventional Commits: `type(scope): summary`.

- `feat`: new behaviour or a user-visible improvement.
- `fix`: a bug fix.
- `perf`: faster or lower-memory behaviour.
- `refactor`: structure-only change with no behaviour change.
- `test`: test-only changes, only when testing is re-enabled.
- `docs`, `chore`, `build`, `ci`, `style`, `revert`: use the standard meaning.

Use these types exactly. Do not invent prefixes such as `plan:`, `status:`,
`dev:` or `autopilot:`. Map planning and design documents to `docs:`,
status-page work to `feat:` or `fix:`, and developer tooling to `chore:`.
User-visible improvements use `feat:`, speed or memory changes use `perf:`,
and structure-only changes use `refactor:`.

Write the summary in the imperative, lowercase after the colon, with no
trailing period. Keep the subject under 72 characters. Merge commits use
`Merge task TNNN`, or the conventional subject for the work.

Never mention autopilot, a coding agent, an AI model, AI, a prompt, or the
agent session in commit messages, merge subjects, tags, release titles or
release notes. Describe the product change. Domain terms such as audio session
or MIDI driver are fine.

## Autopilot sessions

The autopilot is `scripts/autopilot.py`, launched with
`scripts/autopilot-run.sh`, `/autopilot`, or by saying "run autopilot".

- The driver owns the queue, branches, commits, merges and Time Manager
  updates. It reads TODO tasks from the Time Manager `Instrument Hub` list,
  picks the lowest `T` number, and starts one fresh `opencode run` session on
  one `task/<task>` branch.
- A session works on exactly one task. Do not create, switch, merge, rebase
  or delete branches. Do not push. Do not update Time Manager. The driver
  handles all of that.
- The driver merges the branch back into `main` with `--no-ff` only after the
  session reports done and verified. A blocker or failed attempt moves the
  task to `ON_HOLD` and the driver continues with the next TODO task.
- Autopilot sessions may use Playwright MCP for ad-hoc verification of the
  running UI. This is not permission to add test files, test dependencies,
  test targets or test tooling.
- The driver waits for DeepSeek off-peak hours before starting a session and
  does not interrupt a session already running.
- A session writes its result to the path in its brief. Never claim done
  without building the affected target, running the app and recording the
  verification.

## Build budget

Hard limits, not aspirations:

- An incremental app rebuild after a normal source change must stay under
  one minute; stop and fix the build graph if it does not.
- If a change makes it slower, stop and fix the build graph before
  continuing.
- Build one target at a time: `cmake --build build --target instrument-hub`,
  or a specific `--target`. Never build the default `all` target.

## Machine setup: low-latency MIDI

When the developer asks to remove power saving or reports WiFi MIDI lag, use
these system settings. They are not repository changes. Find the WiFi device
with `iw dev`; the examples use `wlo1`.

- WiFi power save, immediate: `sudo iw dev wlo1 set power_save off`.
- Persist it for the active NetworkManager connection:
  `nmcli -t -f NAME,DEVICE connection show --active`, then
  `sudo nmcli connection modify "<name>" wifi.powersave 2`.
- Power profile: `powerprofilesctl set performance`. This normally sets
  `scaling_governor` and `energy_performance_preference` to `performance`.
- If the profile does not change them, write `performance` to
  `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor` and
  `.../energy_performance_preference`.
- Audio and PCIe:
  `echo 0 | sudo tee /sys/module/snd_hda_intel/parameters/power_save`;
  `echo performance | sudo tee /sys/module/pcie_aspm/parameters/policy`.
- USB autosuspend should be `-1`:
  `cat /sys/module/usbcore/parameters/autosuspend`.
- TLP must stay inactive: `systemctl is-active tlp`.

The WiFi/NetworkManager setting persists. The CPU profile may reset on reboot;
check it. The HDA/PCIe sysfs writes reset on reboot. Add a boot service only
when the developer asks for persistence. Do not change these on the
developer's machine unless asked.

## Release build and install

When the developer asks for the latest installed version:

1. Build the UI: `cd ui && pnpm run build`.
2. Configure: `cmake --preset release`.
3. Build explicit targets:
   `cmake --build build-release --target instrument-hub ihub-scanner ihub-sandbox --parallel 12`.
   Never use the default `all` target or `scripts/build.sh`.
4. Install the launcher:
   `scripts/install-desktop-entry.sh build-release`
   (the argument is the build directory name, not `release`).
5. Stop any dev instance first (it owns 8080/8443), then launch detached:
   `gtk-launch instrument-hub`. Prefer the desktop launcher over `setsid` from
   an agent shell, which can be reaped when the shell exits. A paired phone may
   need to reload and rescan the QR after this restart; the desktop stays
   authorized through loopback trust.
6. Verify `curl http://127.0.0.1:8080/state` and
   `curl http://127.0.0.1:8080/pairing`. With LAN on, `8443` must listen.

OpenSSL is required (`libssl-dev` on Ubuntu); the release binary links
`libssl.so.4`/`libcrypto.so.4` on the current machine. The old
`dist/Instrument-Hub-0.3.0-x86_64.AppImage` predates the phone bridge; do not
hand it to another machine. If a portable AppImage is requested, rebuild it
and bundle `libssl`/`libcrypto`; the current AppImage script only bundles
libstdc++ and libgcc.

---

## Code organisation

Small files win. A file you can open and understand without scrolling is
easier to review, easier to debug, and easier for an agent to modify
safely. Big files hide coupling: you scroll for ten minutes, then change
the wrong thing. Optimise for comprehension, not for fewer files.

- One file, one responsibility. On opening it, you should be able to say
  in one sentence what it does: one component, one hook, one model, or one
  C++ class implementation.
- Aim for one screen, roughly 80–120 lines. This is a target, not a gate:
  155 lines with one clear responsibility is fine; 150 lines doing three
  unrelated things is not.
- Prefer more, smaller files over fewer, larger ones. Split by reason to
  change: a route, a panel, a hook, pure model logic, fixtures, assertions.
- Keep genuinely related code together. Do not split a component from its
  private helpers, or a class from its invariants — single responsibility
  is not "the smallest possible file".
- Never grow a file past a screen by appending a new section. When a new
  responsibility appears, give it a file and a name.
- Feature code lives in `ui/src/features/<feature>/`; shell-only UI in
  `ui/src/features/shell/`; public surfaces through `index.ts`.
  C++ uses one class per header, with implementation split across `.cpp`
  files by concern.
- Generated code (`ui/src/api/schema.d.ts`) and `third_party/` are exempt.

C++ formatting is owned by `.clang-format`; run `scripts/format.sh --changed`
before committing instead of aligning code by hand. The React UI is
formatted with Prettier.

---

## Comments

Code must explain itself. Do not add comments that restate what the code
does, narrate history, or hide design rationale in source. Keep only:

- directives required by a tool or language: `// NOLINT`,
  `// clang-format off/on`, `// eslint-disable*`, `// @ts-expect-error`,
  `// prettier-ignore`, `# noqa`, generated-file markers and shebangs;
- annotations a framework or platform API requires to change generated
  code, build behaviour or runtime wiring.

Everything else is removed. Rationale belongs in the code structure or a
task description, not in source. Do not add `TODO`/`FIXME` comments; write
a task in Time Manager instead.

---

## Current phase

Interactive development and bug fixing.

- Fix reported bugs. Do not start new features or refactors unless the human
  explicitly asks.
- Reproduce first: exact steps, expected vs actual, and the log or screenshot.
  Use the developer's running instance; do not start a dev server to reproduce.
- One bug per commit.
- Keep the audio-thread contract and the Launchpad/Linux assumptions.
- Launch the app detached (desktop entry / `gtk-launch`, or a long-lived
  terminal). Starting it with `&` from a short-lived shell is not an app
  crash: it dies on `SIGHUP` when that shell exits.

---

## Guardrails (non-negotiable)

These override convenience, deadlines, and any task wording that appears to
contradict them.

- The autopilot may read only the `time-manager` MCP URL and token from the
  OpenCode config; never print, copy or store that token elsewhere.
- Never push, create remotes, force-push, tag or publish a release unless the
  human explicitly asks for it in the current task. Do not do any of these on
  your own initiative.
- Never delete a directory you did not create in this run.
- If a task needs a new dependency, prefer a header-only or vendored one.
  Ask the human before adding a system dependency.
- Keep the audio callback free of allocation, locks, and logging. This is
  the one rule that will not be relaxed for convenience.
- Every commit is a checkpoint.

---

## Audio-thread contract

Inside `processBlock()` / the audio callback:

- no `new`, `malloc`, or any allocation
- no `std::mutex`, locks, or `SpinLock`
- no `std::string`, no logging, no file I/O
- no graph rewiring; all slots are wired at startup and switching only
  changes which slots process, behind a 2–5 ms gain ramp

Cross-thread state goes through `juce::AbstractFifo` or a lock-free SPSC
ring buffer. Buffers are pre-allocated at max block size. Latency is
queried once per load, not per block.

---

## Locked decisions

These must not be re-litigated:

- JUCE 8 submodule at `third_party/JUCE`; C++20; CMake ≥ 3.22; Ninja or
  Unix Makefiles; `CMAKE_BUILD_TYPE=RelWithDebInfo`.
- No package manager for C++; header-only or vendored dependencies.
- Custom fixed audio graph, not `juce::AudioProcessorGraph`, on the live path.
- One HTTP + WebSocket control server (`cpp-httplib`), bound to
  `127.0.0.1:8080` by default.
- LAN mode adds a second HTTPS/WSS listener on `0.0.0.0:8443` with a generated
  certificate. Never rebind the desktop listener when toggling LAN.
- The phone MIDI bridge is Web MIDI over WSS; there is no native phone app.
- OpenSSL is the only additional system dependency, used for HTTPS/WSS.
- UI and tablet remote are the same React app, different routes.
- Set files are a versioned `.ihub` container, never inline plugin chunks.
- Repo layout: `engine/`, `ui/`, `shared/`, `third_party/`.

---

## Verification

Build the affected target and run the app so the developer can inspect it.

Do not create or run tests, test runners, snapshot harnesses, or any test
tooling. Testing is deferred until the human explicitly asks to re-enable it.

Phone bridge verification requires the developer's Android phone and USB-MIDI
keyboard: enable LAN, scan the QR, turn on WiFi MIDI, switch to the stage,
switch tracks, and confirm the desktop MIDI panel shows `Phone MIDI` / `WiFi`.
Do not claim it verified without that hardware.

---

## Blockers

If something needs hardware, a licence, or an unavailable machine, say so
plainly and stop.

The phone bridge needs an Android phone with USB OTG and a USB-MIDI keyboard.
There is no iPhone path without a native app or RTP-MIDI.
