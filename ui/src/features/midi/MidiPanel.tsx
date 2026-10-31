import type { ReactElement } from "react";

import type { ControlMidiInput, ControlMidiPort, ControlMidiState } from "../../api/client";
import {
    canRunPadTest,
    isLaunchpadDaInput,
    midiInputForRole,
    midiInputState,
    midiInputSummary,
    midiPortLabel,
    midiPortState,
    midiRoleLabel,
    padTestLabel,
} from "./model";
import "./midi.css";

export interface MidiPanelProps
{
    /** Latest `GET /midi`, or null before the first load. */
    readonly midi: ControlMidiState | null;
    /** Message from a failed MIDI request. */
    readonly error?: string | null;
    /** True while an input toggle or reload is in flight. */
    readonly busy?: boolean;
    /** True while the pad test is starting. */
    readonly padTestBusy?: boolean;
    /** Message from a rejected pad test. */
    readonly padTestError?: string | null;
    /** Show a skeleton while the first MIDI state arrives. */
    readonly loading?: boolean;
    /** Called with the new switch state for one input. */
    readonly onToggleInput?: (id: string, enabled: boolean) => void;
    /** Called by the pad-test button. */
    readonly onPadTest?: () => void;
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

function PortCard ({ kind, port, label, hint }: {
    readonly kind: "launchpad" | "keystation";
    readonly port: ControlMidiPort;
    readonly label: string;
    readonly hint: string;
}): ReactElement
{
    return (
        <article
            className={`midi__port${port.connected ? " midi__port--connected" : ""}`}
            data-testid={`${kind}-status`}
            data-connected={port.connected ? "true" : "false"}
            data-port={port.port}
            data-handshake={port.handshake ? "true" : "false"}
        >
            <header className="midi__port-head">
                <h3 className="midi__port-title">{label}</h3>
                <span
                    className={`midi__state midi__state--${port.connected ? "on" : "off"}`}
                    data-testid={`${kind}-state`}
                >
                    {midiPortState (port, port.name.length > 0)}
                </span>
            </header>

            <p className="midi__port-line">
                <span className="midi__port-caption">Port</span>
                <span
                    className="midi__badge"
                    data-testid={`${kind}-port`}
                    data-port={port.port}
                >
                    {midiPortLabel (port.port)}
                </span>
                <span className="midi__port-device" data-testid={`${kind}-device`}>
                    {port.name.length > 0 ? port.name : "No device on the MIDI inputs"}
                </span>
            </p>

            {port.connected
                ? <p className="midi__port-line midi__port-line--muted" data-testid={`${kind}-names`}>
                      in <code>{port.inputName}</code> · out <code>{port.outputName}</code>
                  </p>
                : null}

            {kind === "launchpad"
                ? <p
                      className={`midi__port-line ${port.handshake ? "midi__port-hint--ok" : "midi__port-line--muted"}`}
                      data-testid="launchpad-handshake"
                      data-handshake={port.handshake ? "true" : "false"}
                  >
                      {port.handshake
                          ? "Programmer mode confirmed by the device."
                          : "Waiting for the programmer-mode echo."}
                  </p>
                : null}

            {port.error.length > 0
                ? <p className="midi__port-error" data-testid={`${kind}-error`}>
                      {port.error}
                  </p>
                : null}

            <p className="midi__port-hint">{hint}</p>
        </article>
    );
}

/**
    The MIDI side of the Audio page.

    Lists the ALSA inputs with their enable switches, shows the Launchpad's MI
    port and programming handshake, reports the Keystation, and offers the
    pad test that lights all 64 pads on the real device.
*/
export function MidiPanel ({
    midi,
    error = null,
    busy = false,
    padTestBusy = false,
    padTestError = null,
    loading = false,
    onToggleInput,
    onPadTest,
}: MidiPanelProps): ReactElement
{
    if (loading || midi === null)
    {
        return (
            <section
                className="midi"
                aria-label="MIDI inputs"
                aria-busy="true"
                data-testid="midi-panel"
                data-loading="true"
            >
                <header className="midi__head">
                    <div>
                        <p className="midi__eyebrow">Controllers</p>
                        <h2 className="midi__title">MIDI</h2>
                    </div>
                </header>
                <p className="midi__empty" data-testid="midi-loading">
                    {error !== null && error.length > 0
                        ? error
                        : "Reading the MIDI inputs…"}
                </p>
            </section>
        );
    }

    const launchpad = midiInputForRole (midi.inputs, "launchpad");
    const keystation = midiInputForRole (midi.inputs, "keystation");
    const enabled = midi.inputs.filter (input => input.enabled).length;

    return (
        <section
            className="midi"
            aria-label="MIDI inputs"
            aria-busy={busy}
            data-testid="midi-panel"
            data-input-count={midi.inputs.length}
            data-enabled-count={enabled}
            data-pad-test-active={midi.padTest.active ? "true" : "false"}
        >
            <header className="midi__head">
                <div>
                    <p className="midi__eyebrow">Controllers</p>
                    <h2 className="midi__title">MIDI</h2>
                    <p className="midi__lede">
                        Inputs the engine can see through ALSA. Enable the ones you
                        want open; the Launchpad keeps its MI port for pads and LEDs.
                    </p>
                </div>
                <span className="midi__summary" data-testid="midi-summary">
                    {midiInputSummary (midi.inputs)}
                </span>
            </header>

            {error !== null && error.length > 0
                ? <p className="midi__error" data-testid="midi-error" role="alert">
                      {error}
                  </p>
                : null}

            {midi.error.length > 0
                ? <p className="midi__error" data-testid="midi-device-error" role="alert">
                      {midi.error}
                  </p>
                : null}

            {midi.inputs.length === 0
                ? <p className="midi__empty" data-testid="midi-empty">
                      No MIDI inputs were found. Connect a controller and reload.
                  </p>
                : <ul className="midi__inputs" data-testid="midi-inputs">
                      {midi.inputs.map (input => (
                          <InputRow
                              key={input.id}
                              input={input}
                              busy={busy}
                              onToggle={onToggleInput}
                          />
                      ))}
                  </ul>}

            <div className="midi__ports">
                <PortCard
                    kind="launchpad"
                    port={midi.launchpad}
                    label="Launchpad Mini MK3"
                    hint={launchpad === null
                        ? "No Launchpad input is visible on ALSA."
                        : "Pads and LEDs use the MI port; the DA port carries no grid data."}
                />
                <PortCard
                    kind="keystation"
                    port={midi.keystation}
                    label="Keystation"
                    hint={keystation === null
                        ? "No Keystation input is visible on ALSA."
                        : "Enable the keyboard above to open its USB MIDI port."}
                />
            </div>

            <div className="midi__test" data-testid="pad-test-panel">
                <div className="midi__test-copy">
                    <h3 className="midi__test-title">Pad test</h3>
                    <p className="midi__test-lede">
                        Lights all 64 pads for about a second, then repaints the grid.
                    </p>
                </div>

                <button
                    className="midi__button"
                    type="button"
                    data-testid="pad-test"
                    data-busy={padTestBusy ? "true" : "false"}
                    data-active={midi.padTest.active ? "true" : "false"}
                    disabled={padTestBusy || midi.padTest.active
                        || ! canRunPadTest (midi) || onPadTest === undefined}
                    onClick={() => { onPadTest?.(); }}
                >
                    {padTestBusy || midi.padTest.active ? "Lighting…" : "Light the grid"}
                </button>

                <span
                    className={`midi__test-result${padTestError !== null ? " midi__test-result--error" : ""}`}
                    data-testid="pad-test-result"
                    data-active={midi.padTest.active ? "true" : "false"}
                    data-lit={midi.padTest.lit}
                >
                    {padTestLabel (midi.padTest, padTestError)}
                </span>

                {! canRunPadTest (midi) && padTestError === null
                    ? <span className="midi__test-hint" data-testid="pad-test-hint">
                          Connect the Launchpad and its MI port to run the test.
                      </span>
                    : null}
            </div>
        </section>
    );
}
