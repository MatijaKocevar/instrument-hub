import type { ReactElement } from "react";

import type { ControlPatch } from "../../api/client";
import { clampPan, formatPan } from "./format";

interface LiveTrackChannelProps {
  readonly patch: ControlPatch;
  readonly onPanChange?: (slot: number, pan: number) => void;
  readonly onMuteChange?: (slot: number, muted: boolean) => void;
  readonly onSoloChange?: (slot: number, solo: boolean) => void;
}

export function LiveTrackChannel({
  patch,
  onPanChange,
  onMuteChange,
  onSoloChange,
}: LiveTrackChannelProps): ReactElement {
  const pan = clampPan(patch.pan);

  return (
    <div
      className="instrument__live-channel"
      role="group"
      aria-label="Channel controls"
      data-testid="instrument-mix"
      data-pan={pan.toFixed(2)}
    >
      <div className="instrument__live-channel-head">
        <span className="instrument__live-label">Balance</span>
        <output className="instrument__live-value" data-testid="pan-value">
          {formatPan(pan)}
        </output>
      </div>

      <input
        id="instrument-pan"
        className="instrument__live-fader"
        type="range"
        min={-1}
        max={1}
        step={0.01}
        value={pan}
        aria-label="Balance"
        aria-valuetext={formatPan(pan)}
        data-testid="pan-slider"
        data-slot={patch.slot}
        data-pan={pan.toFixed(2)}
        onChange={(event) => {
          onPanChange?.(patch.slot, Number(event.currentTarget.value));
        }}
      />

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
    </div>
  );
}
