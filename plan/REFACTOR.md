# Refactor plan — single responsibility

`AGENTS.md` § Code organisation is the rule. The executable queue is
`plan/ROADMAP.md`, tasks **T140–T174**.

## Why

Small files are easier to open, review and change without scrolling.
Huge files hide coupling: you scroll for ten minutes, then change the
wrong thing. The one-sentence rule decides a split:

> Can you say what this file does in one sentence?
>
> If not, it has more than one responsibility.

Line count is a signal, not a gate. Aim for one screen (~80–120 lines);
155 lines with one clear responsibility is fine. A 90-line file doing
three things is not.

## Target structure

### UI

```
ui/src/
  App.tsx                  router entry only
  features/
    shell/                 nav, layout, shell state, route selection
    grid/                  launch surface, meters, model
    instrument/            page frame plus one file per card/action
    plugins/               toolbar, inventory, row/card, used-by
    audio-settings/        device, channels, latency, xruns, measure
    midi/                  inputs, port status, pad lighting
    pairing/               code, QR, token sections
    settings/              account card, about card
    remote/                stage layout, set list, controls, link
  state/
    store core + one module per API domain
  api/
    transport core + one module per resource
```

- `index.ts` is the public surface of a feature; no deep imports across
  features.
- A component and its private helpers stay together. A component and a
  route's store wiring do not.

### Engine

- One class per header.
- Implementation may span several `.cpp` files when they have distinct
  concerns (routes, auth, state, meters, WebSocket).
- A `.cpp` file that contains two unrelated classes or two unrelated
  route families gets split.

