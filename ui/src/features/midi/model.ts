import type {
    ControlMidiInput,
    ControlMidiPadTest,
    ControlMidiPort,
    ControlMidiState,
} from "../../api/client";

export type MidiInputRole = "launchpad" | "keystation" | "input";

export function midiRoleLabel (role: string): string
{
    switch (role)
    {
        case "launchpad": return "Launchpad";
        case "keystation": return "Keyboard";
        default: return "MIDI input";
    }
}

export function midiInputState (input: ControlMidiInput): string
{
    if (! input.enabled)
        return "Off";

    return input.open ? "Open" : "Enabled";
}

export function midiInputSummary (inputs: readonly ControlMidiInput[]): string
{
    const enabled = inputs.filter (input => input.enabled).length;
    const noun = inputs.length === 1 ? "input" : "inputs";

    if (inputs.length === 0)
        return "No MIDI inputs";

    return `${String (inputs.length)} ${noun} · ${String (enabled)} enabled`;
}

export function midiPortLabel (port: string): string
{
    return port.trim().length > 0 ? port : "—";
}

export function midiPortState (port: ControlMidiPort, present: boolean): string
{
    if (port.connected)
        return "Connected";

    if (present || port.name.length > 0)
        return "Detected · not connected";

    return "Not detected";
}

export function isLaunchpadDaInput (input: ControlMidiInput): boolean
{
    return input.role === "launchpad" && input.port === "DA";
}

export function midiInputForRole (inputs: readonly ControlMidiInput[],
                                  role: MidiInputRole): ControlMidiInput | null
{
    return inputs.find (input => input.role === role) ?? null;
}

export function padTestLabel (padTest: ControlMidiPadTest | null,
                              error: string | null): string
{
    if (error !== null && error.length > 0)
        return error;

    if (padTest === null)
        return "Not run yet";

    if (padTest.active)
        return `${String (padTest.lit)} pads lit`;

    if (padTest.lit > 0)
        return `Last test lit ${String (padTest.lit)} pads`;

    return "Not run yet";
}

export function canRunPadTest (midi: ControlMidiState | null): boolean
{
    return midi?.launchpad.connected === true;
}
