# AGENTS.md

Operating rules for any agent working in this repository.

Instrument Hub: a JUCE 8 C++20 audio engine with an embedded React UI,
driven by a Novation Launchpad Mini MK3, controlled over a local HTTP +
WebSocket API.

New machine? `docs/new-machine.md` holds the one-time human setup (clone,
toolchain, opencode auth, `IH_REPO`, how to stop a run cleanly).

---

## Working with the developer

The human on this machine is a developer working interactively. They expect
changes to be built and the running app restarted so they can see them.

- After a change, build the affected target and restart the running app
  (engine, Vite, or both) without asking. Reuse the command line of the
  running instance, launch detached (`setsid`; add `pw-jack` when the
  current instance needs JACK), and leave it running for the developer.
- Never run a build without an explicit target. `cmake --build <dir>`,
  bare `ninja`, and `scripts/build.sh` build the whole tree. Always pass
  `--target`.
- The only verification is that the app builds and runs. There is no test
  build in the CMake tree. Do not add, build, or run tests, test runners,
  snapshot harnesses, or test targets.
- Do not run `--selftest`, `--snapshot`, or any test harness. Use the app
  build and the live app.
- Do not build after every edit just for the sake of it. Build when the
  change is ready to run, then restart the app once.

## Build budget

Hard limits, not aspirations:

- An incremental app rebuild after a normal source change must stay under
  one minute; stop and fix the build graph if it does not.
- If a change makes it slower, stop and fix the build graph before
  continuing.
- Build one target at a time: `cmake --build build --target instrument-hub`,
  or a specific `--target`. Never build the default `all` target.

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
  change: a route, a panel, a hook, pure model logic, fixtures,
  assertions.
- Keep genuinely related code together. Do not split a component from its
  private helpers, or a class from its invariants — single responsibility
  is not "the smallest possible file".
- Never grow a file past a screen by appending a new section. When a new
  responsibility appears, give it a file and a name.
- Feature code lives in `ui/src/features/<feature>/`; shell-only UI in
  `ui/src/features/shell/`; public surfaces through `index.ts`.
  C++ uses one class per header, with implementation split across `.cpp`
  files by concern.
- Generated code (`ui/src/api/schema.d.ts`), `third_party/` and build
  dirs are exempt.
- Existing oversized files are tracked in `plan/REFACTOR.md`; do not make
  them worse while fixing bugs, and split them when you touch them.

---

## Comments

Code must explain itself. Do not add comments that restate what the code
does, narrate history, or hide design rationale in source. Keep only:

- directives required by a tool or language: `// NOLINT`,
  `// clang-format off/on`, `// eslint-disable*`, `// @ts-expect-error`,
  `// prettier-ignore`, `# noqa`, generated-file markers and shebangs;
- annotations a framework or platform API requires to change generated
  code, build behaviour or runtime wiring.

Everything else is removed. Rationale belongs in `docs/` or
`plan/DECISIONS.md`. Do not add `TODO`/`FIXME` comments; write a task or a
bug instead.

This applies to first-party source under `engine/`, `ui/src/`, `scripts/`
and build files. `third_party/`, generated code, `docs/` and
`plan/` are exempt. The cleanup queue is `plan/ROADMAP.md` T175–T181.

---

## Current phase: bug fixing

The feature roadmap is complete (`T000–T139`) and the app is playable: the
Launchpad selects patches, the Keystation plays the armed patch, and
per-patch Analog Lab state persists across switches and restarts. From now
on the work is **bug fixing plus the explicitly requested refactors
(T140–T174) and comment cleanup (T175–T181) queued in `plan/ROADMAP.md`**.
`plan/REFACTOR.md` holds the refactor rationale and target structure.

- Fix reported bugs. Do not start new features, refactors or cleanup
  tasks unless the human explicitly asks.
- Reproduce first: exact steps, expected vs actual, and the log or
  screenshot. Record it in `plan/BUGS.md`. Use the developer's running
  instance; do not start a dev server to reproduce.
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
- Repo layout: `engine/`, `ui/`, `shared/`, `third_party/`, `plan/`.

---

## Verification

The only verification is that the app builds and runs: build the affected
target, then restart the app so the developer can inspect it.

Do not build or run tests, test runners, snapshot harnesses, `--selftest` or
`--snapshot`. The test tree is not part of the build.

---

## Blockers

`plan/BLOCKERS.md` is append-only and is the only channel for human work.
If something needs `sudo`, hardware, a licence, or an unavailable machine,
append an entry there and report `STATUS: blocked`; never ask a question
and never work around a guardrail.
