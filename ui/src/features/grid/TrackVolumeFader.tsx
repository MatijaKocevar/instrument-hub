import type { ReactElement } from "react";

import type { GridTrack } from "./trackModel";
import { TrackLevelMeter } from "./TrackLevelMeter";
import {
  TRACK_MAX_DB,
  TRACK_MIN_DB,
  TRACK_VOLUME_TICKS,
  clampTrackVolumeGain,
  trackDbForGain,
  trackVolumePercentForPosition,
  trackVolumePositionForGain,
  trackVolumeStepForGain,
} from "./trackVolume";
import { useTrackVolumeInput } from "./useTrackVolumeInput";

interface TrackVolumeFaderProps {
  readonly track: GridTrack;
  readonly onSetTrackVolume:
    ((index: number, gain: number) => void) | undefined;
}

export function TrackVolumeFader({
  track,
  onSetTrackVolume,
}: TrackVolumeFaderProps): ReactElement {
  const inactive = !track.hasInstrument;
  const gain = clampTrackVolumeGain(track.gain);
  const volumeDb = trackDbForGain(gain);
  const position = trackVolumePositionForGain(gain);
  const step = trackVolumeStepForGain(gain);
  const over = gain > 1 + 1e-6;
  const disabled = inactive || onSetTrackVolume === undefined;
  const dbValue = volumeDb.toFixed(1);
  const readoutDb = `${dbValue} dB`;
  const input = useTrackVolumeInput({
    gain,
    disabled,
    onChange:
      onSetTrackVolume === undefined
        ? undefined
        : (next) => {
            onSetTrackVolume(track.index, next);
          },
  });

  return (
    <div
      className={`track-grid__volume${inactive ? " track-grid__volume--inactive" : ""}${over ? " track-grid__volume--over" : ""}`}
      data-testid="track-fader"
      data-track-index={track.index}
      data-volume-step={step}
      data-gain={gain.toFixed(6)}
      data-db={dbValue}
      data-dragging={input.dragging ? "true" : "false"}
      data-inactive={inactive ? "true" : "false"}
    >
      <span
        className="track-grid__fader-readout"
        data-testid="track-fader-readout"
        data-track-index={track.index}
        data-volume-step={step}
        data-db={dbValue}
      >
        <span className="track-grid__fader-db">{readoutDb}</span>
      </span>

      <div className="track-grid__fader-visual" aria-hidden="true">
        <div className="track-grid__fader-track">
          <span
            className={`track-grid__fader-cap${over ? " track-grid__fader-cap--over" : ""}${gain <= 0 ? " track-grid__fader-cap--bottom" : ""}`}
            data-testid="track-fader-cap"
            data-step={step}
            style={{
              bottom: `${String(trackVolumePercentForPosition(position))}%`,
            }}
          />

          <div className="track-grid__fader-scale">
            {TRACK_VOLUME_TICKS.map((tick) => {
              const reached = !inactive && position + 1e-6 >= tick.position;
              const active = !inactive && Math.abs(volumeDb - tick.db) < 0.05;
              const tickOver = tick.db > 0;

              return (
                <span
                  key={tick.db}
                  className={`track-grid__fader-tick${reached ? " track-grid__fader-tick--reached" : ""}${active ? " track-grid__fader-tick--active" : ""}${tickOver ? " track-grid__fader-tick--over" : ""}`}
                  data-testid="track-fader-tick"
                  data-db={tick.db}
                  data-position={tick.position * 100}
                  data-label={tick.label}
                  data-active={active ? "true" : "false"}
                  data-reached={reached ? "true" : "false"}
                  data-over={tickOver ? "true" : "false"}
                  style={{
                    bottom: `${String(trackVolumePercentForPosition(tick.position))}%`,
                  }}
                >
                  <span className="track-grid__fader-tick-line" />
                  <span className="track-grid__fader-label">{tick.label}</span>
                </span>
              );
            })}
          </div>
        </div>
      </div>

      <div
        className="track-grid__fader-input"
        role="slider"
        tabIndex={disabled ? -1 : 0}
        aria-label={`Track ${String(track.index + 1)} volume`}
        aria-orientation="vertical"
        aria-valuemin={TRACK_MIN_DB}
        aria-valuemax={TRACK_MAX_DB}
        aria-valuenow={Number(volumeDb.toFixed(1))}
        aria-valuetext={inactive ? "unavailable" : readoutDb}
        aria-disabled={disabled}
        data-testid="track-volume"
        data-track-index={track.index}
        data-step={step}
        data-volume-step={step}
        data-gain={gain.toFixed(6)}
        data-db={dbValue}
        data-dragging={input.dragging ? "true" : "false"}
        onPointerDown={input.onPointerDown}
        onPointerMove={input.onPointerMove}
        onPointerUp={input.onPointerUp}
        onPointerCancel={input.onPointerCancel}
        onLostPointerCapture={input.onLostPointerCapture}
        onKeyDown={input.onKeyDown}
      />

      <TrackLevelMeter
        slot={track.slot}
        index={track.index}
        active={!inactive}
      />
    </div>
  );
}
