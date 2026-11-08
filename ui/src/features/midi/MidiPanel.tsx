import type { ReactElement } from "react";

import type {
  ControlMidiInputUpdate,
  ControlMidiState,
} from "../../api/client";
import { MidiInputList } from "./MidiInputList";
import { midiInputSummary, midiLaunchpadSummary } from "./model";
import { useMidiLearnPolling } from "./useMidiLearnPolling";
import "./midi.css";

export interface MidiPanelProps {
  readonly midi: ControlMidiState | null;
  readonly error?: string | null;
  readonly busy?: boolean;
  readonly loading?: boolean;
  readonly onChangeInput?:
    ((patch: ControlMidiInputUpdate) => void) | undefined;
  readonly onRefreshMidi?: (() => void) | undefined;
}

export function MidiPanel({
  midi,
  error = null,
  busy = false,
  loading = false,
  onChangeInput,
  onRefreshMidi,
}: MidiPanelProps): ReactElement {
  const learning = midi?.inputs.some((input) => input.learnActive) ?? false;

  useMidiLearnPolling(learning, onRefreshMidi);

  if (loading || midi === null) {
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

  const enabled = midi.inputs.filter((input) => input.enabled).length;

  return (
    <section
      className="midi"
      aria-label="MIDI inputs"
      aria-busy={busy}
      data-testid="midi-panel"
      data-input-count={midi.inputs.length}
      data-enabled-count={enabled}
    >
      <header className="midi__head">
        <div>
          <p className="midi__eyebrow">Controllers</p>
          <h2 className="midi__title">MIDI</h2>
          <p className="midi__lede">
            Any keyboard plays the armed slot. Switch a controller to Pad grid
            to map its notes onto the 8×8 grid; only the Launchpad drives LEDs.
          </p>
        </div>
        <span className="midi__summary" data-testid="midi-summary">
          {midiInputSummary(midi.inputs)} · {midiLaunchpadSummary(midi.inputs)}
        </span>
      </header>

      {error !== null && error.length > 0 ? (
        <p className="midi__error" data-testid="midi-error" role="alert">
          {error}
        </p>
      ) : null}

      {midi.error.length > 0 ? (
        <p className="midi__error" data-testid="midi-device-error" role="alert">
          {midi.error}
        </p>
      ) : null}

      <MidiInputList
        inputs={midi.inputs}
        busy={busy}
        onChange={onChangeInput}
      />
    </section>
  );
}
