import type {
  ControlMidiInput,
  ControlMidiPadTest,
  ControlMidiState,
} from "../../api/client";

export type MidiInputRole = "launchpad" | "keyboard" | "grid";

export const midiGridPadCount = 64;

export function midiRoleLabel(role: string): string {
  switch (role) {
    case "launchpad":
      return "Launchpad";
    case "grid":
      return "Pad grid";
    default:
      return "Keyboard";
  }
}

export function midiInputState(input: ControlMidiInput): string {
  if (!input.enabled) return "Off";

  return input.open ? "Open" : "Enabled";
}

export function midiHandshakeLabel(input: ControlMidiInput): string {
  if (input.role === "launchpad")
    return input.handshake ? "Programmer mode" : "No handshake";

  if (input.role === "grid") return input.leds ? "LEDs" : "No LED output";

  return "—";
}

export function midiGridLabel(input: ControlMidiInput): string {
  if (input.learnActive)
    return `Learning ${String(input.learnedNotes)} of ${String(midiGridPadCount)} pads`;

  if (input.learnedNotes >= midiGridPadCount) return "64 pads learned";

  if (input.noteBase >= 0)
    return `Notes ${String(input.noteBase)}–${String(input.noteBase + midiGridPadCount - 1)}`;

  return "Not configured";
}

export function midiGridConfigured(input: ControlMidiInput): boolean {
  return input.learnedNotes >= midiGridPadCount || input.noteBase >= 0;
}

export function midiInputSummary(inputs: readonly ControlMidiInput[]): string {
  const enabled = inputs.filter((input) => input.enabled).length;
  const noun = inputs.length === 1 ? "input" : "inputs";

  if (inputs.length === 0) return "No MIDI inputs";

  return `${String(inputs.length)} ${noun} · ${String(enabled)} enabled`;
}

export function midiLaunchpadSummary(
  inputs: readonly ControlMidiInput[],
): string {
  const launchpad =
    inputs.find((input) => input.role === "launchpad" && input.port === "MI") ??
    inputs.find((input) => input.role === "launchpad");

  if (launchpad === undefined) return "No Launchpad";

  if (!launchpad.open) return "Launchpad offline";

  return launchpad.handshake ? "Launchpad ready" : "Launchpad handshake…";
}

export function midiPortLabel(port: string): string {
  return port.trim().length > 0 ? port : "—";
}

export function isLaunchpadDaInput(input: ControlMidiInput): boolean {
  return input.role === "launchpad" && input.port === "DA";
}

export function midiInputForRole(
  inputs: readonly ControlMidiInput[],
  role: MidiInputRole,
): ControlMidiInput | null {
  return inputs.find((input) => input.role === role) ?? null;
}

export function padTestLabel(
  padTest: ControlMidiPadTest | null,
  error: string | null,
): string {
  if (error !== null && error.length > 0) return error;

  if (padTest === null) return "Not run yet";

  if (padTest.active) return `${String(padTest.lit)} pads lit`;

  if (padTest.lit > 0) return `Last test lit ${String(padTest.lit)} pads`;

  return "Not run yet";
}

export function canRunPadTest(midi: ControlMidiState | null): boolean {
  return (
    midi?.inputs.some((input) => input.role === "launchpad" && input.open) ===
    true
  );
}
