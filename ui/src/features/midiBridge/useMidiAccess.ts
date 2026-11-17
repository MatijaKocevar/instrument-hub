import { useCallback, useEffect, useRef, useState } from "react";

import {
  readMidiPorts,
  webMidiSupport,
  type MidiPort,
  type WebMidiSupport,
} from "./webMidi";

export type MidiPermission = "idle" | "requesting" | "ready" | "denied";

export interface MidiAccessState {
  readonly support: WebMidiSupport;
  readonly permission: MidiPermission;
  readonly ports: readonly MidiPort[];
  readonly error: string | null;
  readonly access: MIDIAccess | null;
  readonly request: () => Promise<void>;
}

export function useMidiAccess(): MidiAccessState {
  const support = webMidiSupport();
  const [permission, setPermission] = useState<MidiPermission>("idle");
  const [ports, setPorts] = useState<MidiPort[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [access, setAccess] = useState<MIDIAccess | null>(null);
  const alive = useRef(true);

  useEffect(() => {
    alive.current = true;

    return () => {
      alive.current = false;
    };
  }, []);

  const request = useCallback(async (): Promise<void> => {
    if (support !== "available") return;

    setPermission("requesting");
    setError(null);

    try {
      const value = await navigator.requestMIDIAccess({ sysex: false });

      if (!alive.current) return;

      setAccess(value);
      setPorts(readMidiPorts(value));
      setPermission("ready");

      value.onstatechange = (): void => {
        if (alive.current) setPorts(readMidiPorts(value));
      };
    } catch (cause) {
      if (!alive.current) return;

      setPermission("denied");
      setError(
        cause instanceof Error && cause.message.length > 0
          ? cause.message
          : "MIDI access was denied",
      );
    }
  }, [support]);

  return { support, permission, ports, error, access, request };
}
