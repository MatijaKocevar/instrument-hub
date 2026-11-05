import type { ReactElement } from "react";

import type { GridTrack } from "./trackModel";
import { trackVolumeStep, volumeStepForRow } from "./trackVolume";

interface TrackVolumeFaderProps {
  readonly track: GridTrack;
  readonly step: number;
  readonly onSetTrackVolume:
    ((index: number, step: number) => void) | undefined;
}

export function TrackVolumeFader({
  track,
  step,
  onSetTrackVolume,
}: TrackVolumeFaderProps): ReactElement {
  const inactive = !track.hasInstrument;
  const current = track.volumeStep;
  const active = !inactive && current === step;
  const over = !inactive && step === 4 && current === 4;
  const filled =
    !inactive && (current > step || (current === step && current > 0));
  const cap = current === step;
  const label = trackVolumeStep(step).label;
  const readoutDb = `${track.volumeDb.toFixed(1)} dB`;

  return (
    <div
      className={`grid__cell track-grid__cell track-grid__cell--volume${inactive ? " track-grid__cell--inactive" : ""}${active ? " track-grid__cell--volume-active" : ""}${over ? " track-grid__cell--over" : ""}${filled ? " track-grid__cell--fader-filled" : ""}${cap ? " track-grid__cell--fader-cap" : ""}${cap && step === 0 ? " track-grid__cell--fader-cap-bottom" : ""}`}
      role="gridcell"
      data-testid="track-cell"
      data-track-index={track.index}
      data-row={step}
      data-role="volume"
      data-step={step}
      data-active={active ? "true" : "false"}
      data-over={over ? "true" : "false"}
      data-volume-step={track.volumeStep}
      data-filled={filled ? "true" : "false"}
      data-cap={cap ? "true" : "false"}
      data-inactive={inactive ? "true" : "false"}
    >
      <button
        className="grid__cell-main track-grid__fader"
        type="button"
        data-testid="track-volume"
        data-track-index={track.index}
        data-step={step}
        data-active={active ? "true" : "false"}
        data-over={over ? "true" : "false"}
        aria-pressed={active}
        aria-label={`Track ${String(track.index + 1)} volume ${inactive ? "unavailable" : label}`}
        disabled={inactive || onSetTrackVolume === undefined}
        onClick={
          onSetTrackVolume === undefined
            ? undefined
            : () => {
                onSetTrackVolume(track.index, volumeStepForRow(step));
              }
        }
      >
        {step === 4 ? (
          <span
            className="track-grid__fader-readout"
            data-testid="track-fader-readout"
            data-track-index={track.index}
            data-volume-step={track.volumeStep}
            data-db={track.volumeDb.toFixed(1)}
          >
            <span className="track-grid__fader-db">{readoutDb}</span>
          </span>
        ) : null}
        <span className="track-grid__fader-rail" aria-hidden="true">
          <span
            className="track-grid__fader-fill"
            style={{ height: filled ? "100%" : "0%" }}
          />
        </span>
        {cap ? <span className="track-grid__fader-cap" /> : null}
        <span className="track-grid__fader-tick" aria-hidden="true">
          <span className="track-grid__fader-label">{label}</span>
        </span>
      </button>
    </div>
  );
}
