import { ApiError, type ControlApi, type ControlState } from "../api/client";
import { messageOf } from "./errors";
import type { AppSnapshot } from "./snapshot";
import type { ToastInput } from "./toastTypes";

export interface RefreshDeps {
  readonly api: ControlApi;
  readonly now: () => number;
  readonly getSnapshot: () => AppSnapshot;
  readonly patch: (change: Partial<AppSnapshot>) => void;
  readonly notify: (input: ToastInput) => void;
  readonly resolveToast: (key: string, input: ToastInput) => void;
  readonly preservePending: (state: ControlState) => ControlState;
}

export async function refreshSnapshot(deps: RefreshDeps): Promise<boolean> {
  if (deps.getSnapshot().state === null) deps.patch({ phase: "connecting" });

  const [stateResult, audioResult, pairingResult, midiResult, projectResult] =
    await Promise.allSettled([
      deps.api.getState(),
      deps.api.getAudio(),
      deps.api.getPairing(),
      deps.api.getMidi(),
      deps.api.getProject(),
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

  const prior = deps.getSnapshot();
  const current = prior.state;
  const incoming = stateResult.value;
  const acceptState =
    current === null ||
    incoming.revision >= current.revision ||
    incoming.uptimeMs < current.uptimeMs;

  if (audioResult.status === "rejected") {
    const detail = messageOf(audioResult.reason);

    if (prior.audioError !== detail)
      deps.notify({
        severity: "warning",
        title: "Audio settings unavailable",
        detail,
        key: "load-audio",
      });
  } else if (prior.audioError !== null) {
    deps.resolveToast("load-audio", {
      severity: "success",
      title: "Audio settings restored",
      key: "load-audio",
    });
  }

  if (midiResult.status === "rejected") {
    const detail = messageOf(midiResult.reason);

    if (prior.midiError !== detail)
      deps.notify({
        severity: "warning",
        title: "MIDI inputs unavailable",
        detail,
        key: "load-midi",
      });
  } else if (prior.midiError !== null) {
    deps.resolveToast("load-midi", {
      severity: "success",
      title: "MIDI inputs restored",
      key: "load-midi",
    });
  }

  deps.patch({
    phase: "ready",
    ...(acceptState ? { state: deps.preservePending(incoming) } : {}),
    ...(audioResult.status === "fulfilled"
      ? { audio: audioResult.value, audioError: null }
      : { audioError: messageOf(audioResult.reason) }),
    ...(pairingResult.status === "fulfilled"
      ? { pairing: pairingResult.value }
      : {}),
    ...(midiResult.status === "fulfilled"
      ? { midi: midiResult.value, midiError: null }
      : { midiError: messageOf(midiResult.reason) }),
    ...(projectResult.status === "fulfilled"
      ? { project: projectResult.value }
      : {}),
    error: null,
    unauthorized: false,
    failedAttempts: 0,
    updatedAt: deps.now(),
  });

  return true;
}
