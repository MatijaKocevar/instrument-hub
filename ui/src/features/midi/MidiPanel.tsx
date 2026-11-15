import type { ReactElement } from "react";

import type {
  ControlMidiInputUpdate,
  ControlMidiState,
} from "../../api/client";
import { MidiInputList } from "./MidiInputList";
import { MidiPanelSkeleton } from "./MidiPanelSkeleton";
import { MidiUnavailable } from "./MidiUnavailable";
import { midiInputSummary, midiLaunchpadSummary } from "./model";
import { useMidiLearnPolling } from "./useMidiLearnPolling";
import "./midi.css";

export interface MidiPanelProps {
  readonly midi: ControlMidiState | null;
  readonly unavailable?: boolean;
  readonly busy?: boolean;
  readonly loading?: boolean;
  readonly onChangeInput?:
    ((patch: ControlMidiInputUpdate) => void) | undefined;
  readonly onRefreshMidi?: (() => void) | undefined;
}

export function MidiPanel({
  midi,
  unavailable = false,
  busy = false,
  loading = false,
  onChangeInput,
  onRefreshMidi,
}: MidiPanelProps): ReactElement {
  const learning = midi?.inputs.some((input) => input.learnActive) ?? false;

  useMidiLearnPolling(learning, onRefreshMidi);

  if (loading || midi === null) {
    if (unavailable) return <MidiUnavailable onRetry={onRefreshMidi} />;

    return <MidiPanelSkeleton />;
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

      <MidiInputList
        inputs={midi.inputs}
        busy={busy}
        onChange={onChangeInput}
      />
    </section>
  );
}
