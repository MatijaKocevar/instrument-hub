import type { ReactElement } from "react";

import type { ControlMidiState } from "../../api/client";
import { midiInputSummary } from "./model";
import { MidiInputList } from "./MidiInputList";
import { MidiPortStatus } from "./MidiPortStatus";
import { PadTestPanel } from "./PadTestPanel";
import "./midi.css";

export interface MidiPanelProps {
  readonly midi: ControlMidiState | null;
  readonly error?: string | null;
  readonly busy?: boolean;
  readonly padTestBusy?: boolean;
  readonly padTestError?: string | null;
  readonly loading?: boolean;
  readonly onToggleInput?: (id: string, enabled: boolean) => void;
  readonly onPadTest?: () => void;
}

export function MidiPanel({
  midi,
  error = null,
  busy = false,
  padTestBusy = false,
  padTestError = null,
  loading = false,
  onToggleInput,
  onPadTest,
}: MidiPanelProps): ReactElement {
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
      data-pad-test-active={midi.padTest.active ? "true" : "false"}
    >
      <header className="midi__head">
        <div>
          <p className="midi__eyebrow">Controllers</p>
          <h2 className="midi__title">MIDI</h2>
          <p className="midi__lede">
            Inputs the engine can see through ALSA. Enable the ones you want
            open; the Launchpad keeps its MI port for pads and LEDs.
          </p>
        </div>
        <span className="midi__summary" data-testid="midi-summary">
          {midiInputSummary(midi.inputs)}
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
        onToggle={onToggleInput}
      />

      <MidiPortStatus midi={midi} />

      <PadTestPanel
        midi={midi}
        busy={padTestBusy}
        error={padTestError}
        onPadTest={onPadTest}
      />
    </section>
  );
}
