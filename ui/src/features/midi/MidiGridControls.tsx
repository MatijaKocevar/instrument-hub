import { useEffect, useState } from "react";
import type { ReactElement } from "react";

import type {
  ControlMidiInput,
  ControlMidiInputUpdate,
} from "../../api/client";
import { midiGridLabel, midiGridPadCount } from "./model";

interface MidiGridControlsProps {
  readonly input: ControlMidiInput;
  readonly busy: boolean;
  readonly onChange?: ((patch: ControlMidiInputUpdate) => void) | undefined;
}

export function MidiGridControls({
  input,
  busy,
  onChange,
}: MidiGridControlsProps): ReactElement {
  const [base, setBase] = useState(
    input.noteBase >= 0 ? String(input.noteBase) : "",
  );

  useEffect(() => {
    setBase(input.noteBase >= 0 ? String(input.noteBase) : "");
  }, [input.id, input.noteBase, input.learnedNotes]);

  const parsed = Number.parseInt(base, 10);
  const valid =
    base.trim().length === 0 ||
    (Number.isInteger(parsed) && parsed >= 0 && parsed <= midiGridPadCount);

  const learning = input.learnActive;

  return (
    <div
      className="midi__grid"
      data-testid="midi-grid-controls"
      data-id={input.id}
      data-learn-active={learning ? "true" : "false"}
    >
      <label className="midi__grid-field">
        <span>Note base</span>
        <input
          type="number"
          min={0}
          max={midiGridPadCount}
          step={1}
          value={base}
          disabled={busy || learning}
          data-testid="midi-grid-base"
          onChange={(event) => {
            setBase(event.currentTarget.value);
          }}
        />
      </label>

      <button
        className="midi__mini"
        type="button"
        data-testid="midi-grid-base-apply"
        disabled={busy || learning || !valid || onChange === undefined}
        onClick={() => {
          onChange?.({
            id: input.id,
            noteBase: base.trim().length === 0 ? -1 : parsed,
          });
        }}
      >
        Apply
      </button>

      <button
        className="midi__mini midi__mini--accent"
        type="button"
        data-testid="midi-grid-learn"
        data-active={learning ? "true" : "false"}
        disabled={busy || onChange === undefined}
        onClick={() => {
          onChange?.({
            id: input.id,
            learn: !learning,
          });
        }}
      >
        {learning
          ? "Cancel learn"
          : input.learnedNotes >= midiGridPadCount
            ? "Re-learn"
            : "Learn"}
      </button>

      <span
        className="midi__grid-status"
        data-testid="midi-grid-status"
        data-learned={input.learnedNotes}
      >
        {midiGridLabel(input)}
      </span>

      {learning ? (
        <span className="midi__grid-hint" data-testid="midi-grid-hint">
          Press the 64 pads from bottom-left to top-right.
        </span>
      ) : null}
    </div>
  );
}
