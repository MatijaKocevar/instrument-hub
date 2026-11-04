import type { ReactElement } from "react";

import type {
  ControlMidiInput,
  ControlMidiInputUpdate,
} from "../../api/client";
import { MidiInputRow } from "./MidiInputRow";

interface MidiInputListProps {
  readonly inputs: readonly ControlMidiInput[];
  readonly busy: boolean;
  readonly onChange?: ((patch: ControlMidiInputUpdate) => void) | undefined;
}

export function MidiInputList({
  inputs,
  busy,
  onChange,
}: MidiInputListProps): ReactElement {
  if (inputs.length === 0) {
    return (
      <p className="midi__empty" data-testid="midi-empty">
        No MIDI inputs were found. Connect a controller and reload.
      </p>
    );
  }

  return (
    <ul className="midi__inputs" data-testid="midi-inputs">
      {inputs.map((input) => (
        <MidiInputRow
          key={input.id}
          input={input}
          busy={busy}
          onChange={onChange}
        />
      ))}
    </ul>
  );
}
