import type { ReactElement } from "react";

import type {
  ControlMidiInput,
  ControlMidiInputUpdate,
} from "../../api/client";
import { MidiGridControls } from "./MidiGridControls";
import {
  isLaunchpadDaInput,
  midiHandshakeLabel,
  midiInputState,
  midiPortLabel,
  midiRoleLabel,
} from "./model";

interface MidiInputRowProps {
  readonly input: ControlMidiInput;
  readonly busy: boolean;
  readonly onChange?: ((patch: ControlMidiInputUpdate) => void) | undefined;
}

export function MidiInputRow({
  input,
  busy,
  onChange,
}: MidiInputRowProps): ReactElement {
  const inert = isLaunchpadDaInput(input);
  const launchpad = input.role === "launchpad";

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
      data-handshake={input.handshake ? "true" : "false"}
      data-leds={input.leds ? "true" : "false"}
      data-learn-active={input.learnActive ? "true" : "false"}
    >
      <div className="midi__input-label">
        <input
          id={`midi-toggle-${input.id}`}
          type="checkbox"
          data-testid="midi-input-toggle"
          data-id={input.id}
          checked={input.enabled}
          disabled={busy || inert || onChange === undefined}
          title={
            inert
              ? "The DA port carries no pads or LEDs; the grid uses MI."
              : input.name
          }
          onChange={(event) => {
            onChange?.({ id: input.id, enabled: event.currentTarget.checked });
          }}
        />

        <span className="midi__input-main">
          <label
            className="midi__input-name"
            data-testid="midi-input-name"
            htmlFor={`midi-toggle-${input.id}`}
          >
            {input.name}
          </label>
          <span className="midi__input-meta">
            {launchpad ? (
              <span className="midi__role" data-testid="midi-input-role">
                {midiRoleLabel(input.role)}
              </span>
            ) : (
              <select
                className="midi__role-select"
                data-testid="midi-input-role"
                data-id={input.id}
                value={input.role}
                disabled={busy || onChange === undefined}
                title="Use this input as a keyboard or as an 8x8 pad grid"
                onChange={(event) => {
                  const role = event.currentTarget.value;

                  if (role === "grid")
                    onChange?.({ id: input.id, role: "grid", enabled: true });
                  else onChange?.({ id: input.id, role: "keyboard" });
                }}
              >
                <option value="keyboard">Keyboard</option>
                <option value="grid">Pad grid</option>
              </select>
            )}

            {input.port.length > 0 ? (
              <span className="midi__role" data-testid="midi-input-port">
                {midiPortLabel(input.port)}
              </span>
            ) : null}

            <span className="midi__input-id" title={input.id}>
              {input.id}
            </span>
          </span>
        </span>
      </div>

      <span className="midi__input-side">
        <span
          className={`midi__state midi__state--${input.enabled ? "on" : "off"}`}
          data-testid="midi-input-state"
        >
          {inert ? "MI only" : midiInputState(input)}
        </span>
        <span
          className={`midi__handshake${input.handshake ? " midi__handshake--ok" : ""}`}
          data-testid="midi-input-handshake"
          data-handshake={input.handshake ? "true" : "false"}
          title="Protocol handshake status"
        >
          {midiHandshakeLabel(input)}
        </span>
        {input.leds ? (
          <span className="midi__leds" data-testid="midi-input-leds">
            LEDs
          </span>
        ) : null}
        {input.events > 0 ? (
          <span className="midi__events" data-testid="midi-input-events">
            {input.events.toLocaleString("en-US")} msg
          </span>
        ) : null}
      </span>

      {input.role === "grid" ? (
        <MidiGridControls input={input} busy={busy} onChange={onChange} />
      ) : null}

      {input.lastEvent.length > 0 ? (
        <span className="midi__last" data-testid="midi-input-last">
          {input.lastEvent}
        </span>
      ) : null}
    </li>
  );
}
