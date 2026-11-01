import type { ReactElement } from "react";

import type { ControlPatch } from "../../api/client";
import {
  FADER_MAX_DB,
  FADER_MIN_DB,
  FADER_STEP_DB,
  clampDb,
  clampPan,
  formatDb,
  formatPan,
  gainFromDb,
} from "./format";

interface MixerProps {
  readonly patch: ControlPatch;
  readonly onVolumeChange?: (slot: number, volumeDb: number) => void;
  readonly onPanChange?: (slot: number, pan: number) => void;
  readonly onMuteChange?: (slot: number, muted: boolean) => void;
  readonly onSoloChange?: (slot: number, solo: boolean) => void;
}

export function Mixer({
  patch,
  onVolumeChange,
  onPanChange,
  onMuteChange,
  onSoloChange,
}: MixerProps): ReactElement {
  const volumeDb = clampDb(patch.volumeDb);
  const pan = clampPan(patch.pan);

  return (
    <section
      className="instrument__card instrument__mix"
      aria-label="Patch mixer"
      data-testid="instrument-mix"
      data-volume-db={volumeDb.toFixed(2)}
      data-gain={gainFromDb(volumeDb).toFixed(6)}
      data-pan={pan.toFixed(2)}
    >
      <header className="instrument__card-head">
        <h3 className="instrument__card-title">Level</h3>
        <span className="instrument__card-hint">Audible on the next block</span>
      </header>

      <label className="instrument__field" htmlFor="instrument-volume">
        <span className="instrument__field-label">Volume</span>
        <input
          id="instrument-volume"
          className="instrument__fader"
          type="range"
          min={FADER_MIN_DB}
          max={FADER_MAX_DB}
          step={FADER_STEP_DB}
          value={volumeDb}
          data-testid="volume-fader"
          data-slot={patch.slot}
          data-db={volumeDb.toFixed(2)}
          aria-valuetext={formatDb(volumeDb)}
          onChange={(event) => {
            onVolumeChange?.(patch.slot, Number(event.currentTarget.value));
          }}
        />
        <output className="instrument__value" data-testid="volume-value">
          {formatDb(volumeDb)}
        </output>
      </label>

      <p
        className="instrument__field-hint"
        data-testid="volume-gain"
        data-gain={gainFromDb(volumeDb).toFixed(6)}
      >
        effective gain ×{gainFromDb(volumeDb).toFixed(3)}
      </p>

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
