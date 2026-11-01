import { ApiError, type ControlApi, type ControlState } from "../api/client";
import { messageOf } from "./errors";
import type { AppSnapshot } from "./snapshot";

export interface RefreshDeps {
  readonly api: ControlApi;
  readonly now: () => number;
  readonly getSnapshot: () => AppSnapshot;
  readonly patch: (change: Partial<AppSnapshot>) => void;
  readonly preservePending: (state: ControlState) => ControlState;
}

export async function refreshSnapshot(deps: RefreshDeps): Promise<boolean> {
  if (deps.getSnapshot().state === null) deps.patch({ phase: "connecting" });

  const [stateResult, audioResult, pairingResult, midiResult] =
    await Promise.allSettled([
      deps.api.getState(),
      deps.api.getAudio(),
      deps.api.getPairing(),
      deps.api.getMidi(),
    ]);

  if (stateResult.status === "rejected") {
    deps.patch({
      phase: "offline",
      error: messageOf(stateResult.reason),
      audioError:
        audioResult.status === "rejected"
          ? messageOf(audioResult.reason)
          : null,
      midiError:
        midiResult.status === "rejected" ? messageOf(midiResult.reason) : null,
      unauthorized:
        stateResult.reason instanceof ApiError &&
        stateResult.reason.status === 401,
      failedAttempts: deps.getSnapshot().failedAttempts + 1,
    });

    return false;
  }

  deps.patch({
    phase: "ready",
    state: deps.preservePending(stateResult.value),
    ...(audioResult.status === "fulfilled"
      ? { audio: audioResult.value, audioError: null }
      : { audioError: messageOf(audioResult.reason) }),
    ...(pairingResult.status === "fulfilled"
      ? { pairing: pairingResult.value }
      : {}),
    ...(midiResult.status === "fulfilled"
      ? { midi: midiResult.value, midiError: null }
      : { midiError: messageOf(midiResult.reason) }),
    error: null,
    unauthorized: false,
    failedAttempts: 0,
    updatedAt: deps.now(),
  });

  return true;
}
