import { useEffect, useRef } from "react";

import type { ControlMidiState } from "../../api/client";
import { useAppStore } from "../../state/AppStoreContext";

interface InputError {
  readonly name: string;
  readonly error: string;
}

const DEVICE_KEY = "midi-device";

export function useMidiStatusToasts(midi: ControlMidiState | null): void {
  const store = useAppStore();
  const previousDevice = useRef("");
  const previousInputs = useRef(new Map<string, InputError>());

  useEffect(() => {
    const deviceError = midi?.error ?? "";
    const priorDevice = previousDevice.current;
    previousDevice.current = deviceError;

    if (deviceError.length > 0 && priorDevice !== deviceError) {
      store.notify({
        severity: "warning",
        title: "MIDI device problem",
        detail: deviceError,
        key: DEVICE_KEY,
        sticky: true,
      });
    } else if (priorDevice.length > 0 && deviceError.length === 0) {
      store.resolveToast(DEVICE_KEY, {
        severity: "success",
        title: "MIDI ready",
        key: DEVICE_KEY,
      });
    }

    const next = new Map<string, InputError>();

    for (const input of midi?.inputs ?? []) {
      if (input.error.length > 0)
        next.set(input.id, { name: input.name, error: input.error });
    }

    for (const [id, input] of next) {
      const key = `midi-input:${id}`;
      const prior = previousInputs.current.get(id);

      if (prior?.error === input.error) continue;

      store.notify({
        severity: "warning",
        title: `MIDI input · ${input.name}`,
        detail: input.error,
        key,
        sticky: true,
      });
    }

    for (const id of previousInputs.current.keys()) {
      if (!next.has(id)) store.dismissToastKey(`midi-input:${id}`);
    }

    previousInputs.current = next;
  }, [midi, store]);
}
