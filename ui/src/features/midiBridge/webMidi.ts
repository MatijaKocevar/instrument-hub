export type WebMidiSupport = "available" | "insecure" | "unsupported";

export interface MidiPort {
  readonly id: string;
  readonly name: string;
  readonly manufacturer: string;
  readonly state: string;
}

export function webMidiSupport(): WebMidiSupport {
  if (typeof navigator === "undefined" || typeof window === "undefined")
    return "unsupported";

  if (!window.isSecureContext) return "insecure";

  return "requestMIDIAccess" in navigator ? "available" : "unsupported";
}

export function readMidiPorts(access: MIDIAccess): MidiPort[] {
  return Array.from(access.inputs.values()).map((input) => ({
    id: input.id,
    name: input.name ?? "",
    manufacturer: input.manufacturer ?? "",
    state: input.state,
  }));
}

export function midiPortLabel(port: MidiPort): string {
  if (port.name.length > 0) return port.name;

  if (port.manufacturer.length > 0) return port.manufacturer;

  return port.id;
}

export function describeMidiData(data: Uint8Array): string {
  if (data.length === 0) return "empty message";

  const status = data[0] ?? 0;
  const type = status & 0xf0;
  const channel = (status & 0x0f) + 1;
  const first = data[1] ?? 0;
  const second = data[2] ?? 0;

  if (type === 0x90 && data.length >= 3)
    return second > 0
      ? `Note on ${String(first)} · ch ${String(channel)}`
      : `Note off ${String(first)} · ch ${String(channel)}`;

  if (type === 0x80 && data.length >= 3)
    return `Note off ${String(first)} · ch ${String(channel)}`;

  if (type === 0xb0 && data.length >= 3)
    return `CC ${String(first)} = ${String(second)} · ch ${String(channel)}`;

  if (type === 0xe0 && data.length >= 3)
    return `Pitch bend · ch ${String(channel)}`;

  return `${String(data.length)} bytes`;
}
