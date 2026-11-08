import type { ReactElement } from "react";

import type { ControlPatch } from "../../api/client";
import { clampPan, formatPan } from "./format";

interface MixerProps {
  readonly patch: ControlPatch;
  readonly onPanChange?: (slot: number, pan: number) => void;
  readonly onMuteChange?: (slot: number, muted: boolean) => void;
  readonly onSoloChange?: (slot: number, solo: boolean) => void;
}

export function Mixer({
  patch,
  onPanChange,
  onMuteChange,
  onSoloChange,
}: MixerProps): ReactElement {
  const pan = clampPan(patch.pan);

  return (
    <section
      className="instrument__card instrument__mix"
      aria-label="Patch channel controls"
      data-testid="instrument-mix"
      data-pan={pan.toFixed(2)}
    >
      <header className="instrument__card-head">
        <h3 className="instrument__card-title">Channel</h3>
      </header>

      <label className="instrument__field" htmlFor="instrument-pan">
        <span className="instrument__field-label">Balance</span>
        <input
          id="instrument-pan"
          className="instrument__fader instrument__fader--pan"
          type="range"
          min={-1}
          max={1}
          step={0.01}
          value={pan}
          data-testid="pan-slider"
          data-slot={patch.slot}
          data-pan={pan.toFixed(2)}
          aria-valuetext={formatPan(pan)}
          onChange={(event) => {
            onPanChange?.(patch.slot, Number(event.currentTarget.value));
          }}
        />
        <output className="instrument__value" data-testid="pan-value">
          {formatPan(pan)}
        </output>
      </label>

      <div
        className="instrument__toggles"
        role="group"
        aria-label="Mute and solo"
      >
        <button
          className={`instrument__toggle${patch.mute ? " instrument__toggle--on" : ""}`}
          type="button"
          aria-pressed={patch.mute}
          data-testid="mute-button"
          data-slot={patch.slot}
          data-muted={patch.mute ? "true" : "false"}
          onClick={() => {
            onMuteChange?.(patch.slot, !patch.mute);
          }}
        >
          Mute
        </button>
        <button
          className={`instrument__toggle${patch.solo ? " instrument__toggle--on" : ""}`}
          type="button"
          aria-pressed={patch.solo}
          data-testid="solo-button"
          data-slot={patch.slot}
          data-solo={patch.solo ? "true" : "false"}
          onClick={() => {
            onSoloChange?.(patch.slot, !patch.solo);
          }}
        >
          Solo
        </button>
      </div>
    </section>
  );
}
