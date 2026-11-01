# AGENTS.md

Operating rules for any agent working in this repository.

Instrument Hub: a JUCE 8 C++20 audio engine with an embedded React UI,
driven by a Novation Launchpad Mini MK3, controlled over a local HTTP +
WebSocket API.

New machine? `docs/new-machine.md` holds the one-time human setup (clone,
toolchain, opencode auth, `IH_REPO`, how to stop a run cleanly).

---

## Current phase: bug fixing

The feature roadmap is complete (`T000–T139`) and the app is playable: the
Launchpad selects patches, the Keystation plays the armed patch, and
per-patch Analog Lab state persists across switches and restarts. From now
on the work is **bug fixing**, not feature work or refactoring.

- Fix reported bugs. Do not start new features, refactors or cleanup
  tasks unless the human explicitly asks.
- Reproduce first: exact steps, expected vs actual, and the log or
  screenshot. Record it in `plan/BUGS.md`.
- Add a regression test where practical. A fix is not done until
  `scripts/test.sh` passes.
- One bug per commit; update the entry's status and evidence.
- Keep the audio-thread contract and the Launchpad/Linux assumptions.
- Launch the app detached (desktop entry, `setsid`, or a long-lived
  terminal). Starting it with `&` from a short-lived shell is not an app
  crash: it dies on `SIGHUP` when that shell exits.

---

## Guardrails (non-negotiable)

Copied from `plan/AUTOPILOT.md` §5. These override convenience, deadlines,
and any task wording that appears to contradict them.

- Work only inside `REPO`. Never `sudo`, never touch `~/.ssh`, `~/.config`,
  or `/etc`.
- Never `git push`, never create remotes, never force-push.
- Never delete a directory you did not create in this run.
- If a task needs a new dependency, prefer a header-only or vendored one.
  Ask for `apt` only through `BLOCKERS.md`.
- Keep the audio callback free of allocation, locks, and logging. This is
  the one rule that will not be relaxed for convenience.
- Every commit is a checkpoint. The driver commits after each `done`.

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

`plan/DECISIONS.md` is authoritative and must not be re-litigated:

- JUCE 8 submodule at `third_party/JUCE`; C++20; CMake ≥ 3.22; Ninja or
  Unix Makefiles; `CMAKE_BUILD_TYPE=RelWithDebInfo`.
- No package manager for C++; header-only or vendored dependencies.
- Custom fixed audio graph, not `juce::AudioProcessorGraph`, on the live path.
- One HTTP + WebSocket control server (`cpp-httplib`), bound to
  `127.0.0.1:8080` by default.
- UI and tablet remote are the same React app, different routes.
- Set files are a versioned `.ihub` container, never inline plugin chunks.
- Repo layout: `engine/`, `ui/`, `shared/`, `third_party/`, `tests/`, `plan/`.

---

## Verification

`plan/TESTING.md` defines how to prove things without hardware: ALSA
loopback for audio, a virtual Launchpad on the ALSA sequencer for MIDI,
an in-repo `TestPlugin` VST3, and snapshots taken inside JUCE. A change is
not done until its acceptance check has been run and its output recorded.

Run tests through `scripts/test.sh` (or `ctest --timeout 120`). A deadlocked
test must fail within two minutes; never leave a test or test wrapper
unbounded, and never treat "the command is still running" as progress.

---

## Blockers

`plan/BLOCKERS.md` is append-only and is the only channel for human work.
If something needs `sudo`, hardware, a licence, or an unavailable machine,
append an entry there and report `STATUS: blocked`; never ask a question
and never work around a guardrail.
