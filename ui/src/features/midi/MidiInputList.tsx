import type { ReactElement } from "react";

import type { ControlMidiInput } from "../../api/client";
import { isLaunchpadDaInput, midiInputState, midiRoleLabel } from "./model";

interface MidiInputListProps
{
    /** Every ALSA input the engine reported. */
    readonly inputs: readonly ControlMidiInput[];
    /** True while an input toggle or reload is in flight. */
    readonly busy: boolean;
    /** Called with the new switch state for one input. */
    readonly onToggle?: ((id: string, enabled: boolean) => void) | undefined;
}

function InputRow ({ input, busy, onToggle }: {
    readonly input: ControlMidiInput;
    readonly busy: boolean;
    readonly onToggle?: ((id: string, enabled: boolean) => void) | undefined;
}): ReactElement
{
    const inert = isLaunchpadDaInput (input);

    return (
        <li
            className={`midi__input${input.enabled ? " midi__input--enabled" : ""}${inert ? " midi__input--inert" : ""}`}
            data-testid="midi-input"
            data-id={input.id}
            data-name={input.name}
            data-role={input.role}
            data-port={input.port}
            data-enabled={input.enabled ? "true" : "false"}
            data-open={input.open ? "true" : "false"}
        >
            <label className="midi__input-label">
                <input
                    type="checkbox"
                    data-testid="midi-input-toggle"
                    data-id={input.id}
                    checked={input.enabled}
                    disabled={busy || inert || onToggle === undefined}
                    title={inert
                        ? "The DA port carries no pads or LEDs; the grid uses MI."
                        : input.name}
                    onChange={event => {
                        onToggle?.(input.id, event.currentTarget.checked);
                    }}
                />
                <span className="midi__input-main">
                    <span className="midi__input-name" data-testid="midi-input-name">
                        {input.name}
                    </span>
                    <span className="midi__input-meta">
                        <span className="midi__role" data-testid="midi-input-role">
                            {midiRoleLabel (input.role)}
                        </span>
                        {input.port.length > 0
                            ? <span className="midi__role" data-testid="midi-input-port">
                                  {input.port}
                              </span>
                            : null}
                        <span className="midi__input-id" title={input.id}>{input.id}</span>
                    </span>
                </span>
            </label>

            <span className="midi__input-side">
                <span
                    className={`midi__state midi__state--${input.enabled ? "on" : "off"}`}
                    data-testid="midi-input-state"
                >
                    {inert ? "MI only" : midiInputState (input)}
                </span>
                {input.events > 0
                    ? <span className="midi__events" data-testid="midi-input-events">
                          {input.events.toLocaleString ("en-US")} msg
                      </span>
                    : null}
            </span>

            {input.lastEvent.length > 0
                ? <span className="midi__last" data-testid="midi-input-last">
                      {input.lastEvent}
                  </span>
                : null}
        </li>
    );
}

/** The ALSA inputs with their enable switches, or the empty-device notice. */
export function MidiInputList ({ inputs, busy, onToggle }: MidiInputListProps): ReactElement
{
    if (inputs.length === 0)
    {
        return (
            <p className="midi__empty" data-testid="midi-empty">
                No MIDI inputs were found. Connect a controller and reload.
            </p>
        );
    }

    return (
        <ul className="midi__inputs" data-testid="midi-inputs">
            {inputs.map (input => (
                <InputRow
                    key={input.id}
                    input={input}
                    busy={busy}
                    onToggle={onToggle}
                />
            ))}
        </ul>
    );
}
