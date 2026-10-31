import type {
    ControlMidiInput,
    ControlMidiPadTest,
    ControlMidiPort,
    ControlMidiState,
} from "../../api/client";

/** What the engine uses a MIDI input for. */
export type MidiInputRole = "launchpad" | "keystation" | "input";

/** Human label for an input role. */
export function midiRoleLabel (role: string): string
{
    switch (role)
    {
        case "launchpad": return "Launchpad";
        case "keystation": return "Keyboard";
        default: return "MIDI input";
    }
}

/** Short status for one input row. */
export function midiInputState (input: ControlMidiInput): string
{
    if (! input.enabled)
        return "Off";

    return input.open ? "Open" : "Enabled";
}

/** "3 inputs · 1 enabled" for the panel heading. */
export function midiInputSummary (inputs: readonly ControlMidiInput[]): string
{
    const enabled = inputs.filter (input => input.enabled).length;
    const noun = inputs.length === 1 ? "input" : "inputs";

    if (inputs.length === 0)
        return "No MIDI inputs";

    return `${String (inputs.length)} ${noun} · ${String (enabled)} enabled`;
}

/** The port label, falling back to a dash rather than an empty pill. */
export function midiPortLabel (port: string): string
{
    return port.trim().length > 0 ? port : "—";
}

/** Launchpad / Keystation connection wording. */
export function midiPortState (port: ControlMidiPort, present: boolean): string
{
    if (port.connected)
        return "Connected";

    if (present || port.name.length > 0)
        return "Detected · not connected";

    return "Not detected";
}

/** The Launchpad DA port is inert; pads and LEDs live on MI. */
export function isLaunchpadDaInput (input: ControlMidiInput): boolean
{
    return input.role === "launchpad" && input.port === "DA";
}

/** First input with a role, used for the controller cards. */
export function midiInputForRole (inputs: readonly ControlMidiInput[],
                                  role: MidiInputRole): ControlMidiInput | null
{
    return inputs.find (input => input.role === role) ?? null;
}

/** Pad-test result wording. */
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

/** True when the MIDI state carries a launchpad that can be tested. */
export function canRunPadTest (midi: ControlMidiState | null): boolean
{
    return midi?.launchpad.connected === true;
}
