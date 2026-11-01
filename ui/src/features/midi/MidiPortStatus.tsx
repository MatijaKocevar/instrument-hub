import type { ReactElement } from "react";

import type { ControlMidiPort, ControlMidiState } from "../../api/client";
import { midiInputForRole, midiPortLabel, midiPortState } from "./model";

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

/** Launchpad and Keystation connection cards, with their ALSA port hints. */
export function MidiPortStatus ({ midi }: { readonly midi: ControlMidiState }): ReactElement
{
    const launchpad = midiInputForRole (midi.inputs, "launchpad");
    const keystation = midiInputForRole (midi.inputs, "keystation");

    return (
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
    );
}
