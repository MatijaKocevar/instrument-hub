import { useCallback, useEffect, useMemo, useState } from "react";

import { loadMidiClientId } from "./clientId";
import { MidiTelemetry } from "./midiTelemetry";
import { useMidiAccess, type MidiPermission } from "./useMidiAccess";
import { useMidiBridgeLink, type BridgeLinkStatus } from "./useMidiBridgeLink";
import type { MidiPort, WebMidiSupport } from "./webMidi";

export interface MidiBridgeController {
  readonly clientId: string;
  readonly support: WebMidiSupport;
  readonly permission: MidiPermission;
  readonly ports: readonly MidiPort[];
  readonly selectedId: string | null;
  readonly error: string | null;
  readonly bridgeOn: boolean;
  readonly linkStatus: BridgeLinkStatus;
  readonly linkError: string | null;
  readonly telemetry: MidiTelemetry;
  readonly requestAccess: () => Promise<void>;
  readonly selectPort: (id: string | null) => void;
  readonly setBridgeOn: (enabled: boolean) => void;
  readonly panic: () => void;
  readonly reconnect: () => void;
}

export interface MidiBridgeControllerOptions {
  readonly baseUrl: string;
  readonly token: string | null;
}

export function useMidiBridgeController(
  options: MidiBridgeControllerOptions,
): MidiBridgeController {
  const clientId = useMemo(() => loadMidiClientId(), []);
  const access = useMidiAccess();
  const telemetry = useMemo(() => new MidiTelemetry(), []);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [bridgeOn, setBridgeOnState] = useState(false);

  const link = useMidiBridgeLink({
    baseUrl: options.baseUrl,
    token: options.token,
    clientId,
    name: "Phone MIDI",
    active: bridgeOn,
  });

  useEffect(() => {
    if (access.ports.length === 0) {
      setSelectedId(null);
      return;
    }

    setSelectedId((current) =>
      current !== null && access.ports.some((port) => port.id === current)
        ? current
        : (access.ports[0]?.id ?? null),
    );
  }, [access.ports]);

  const sendMidi = link.sendMidi;

  useEffect(() => {
    if (
      !bridgeOn ||
      access.permission !== "ready" ||
      selectedId === null ||
      access.access === null
    )
      return;

    const input = access.access.inputs.get(selectedId);

    if (input === undefined) return;

    const onMessage = (event: MIDIMessageEvent): void => {
      const data = event.data;

      if (data === null) return;

      if (!sendMidi(data)) return;

      telemetry.record(data);
    };

    input.addEventListener("midimessage", onMessage);

    return () => {
      input.removeEventListener("midimessage", onMessage);
    };
  }, [
    access.access,
    access.permission,
    bridgeOn,
    selectedId,
    sendMidi,
    telemetry,
  ]);

  const setBridgeOn = useCallback(
    (enabled: boolean): void => {
      setBridgeOnState(enabled);

      if (!enabled) telemetry.reset("Bridge off");
    },
    [telemetry],
  );

  const panic = useCallback((): void => {
    for (let channel = 0; channel < 16; channel += 1) {
      link.sendMidi(new Uint8Array([0xb0 | channel, 64, 0]));
      link.sendMidi(new Uint8Array([0xb0 | channel, 123, 0]));
      link.sendMidi(new Uint8Array([0xb0 | channel, 120, 0]));
    }

    telemetry.reset("All notes off");
  }, [link.sendMidi, telemetry]);

  return {
    clientId,
    support: access.support,
    permission: access.permission,
    ports: access.ports,
    selectedId,
    error: access.error,
    bridgeOn,
    linkStatus: link.status,
    linkError: link.error,
    telemetry,
    requestAccess: access.request,
    selectPort: setSelectedId,
    setBridgeOn,
    panic,
    reconnect: link.reconnect,
  };
}
