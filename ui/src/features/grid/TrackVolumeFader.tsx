import type { ReactElement } from "react";

import type { GridTrack } from "./trackModel";
import { TRACK_GRID_ROWS } from "./trackModel";
import { TrackLevelMeter } from "./TrackLevelMeter";
import {
  TRACK_VOLUME_STEPS,
  trackVolumeStageCenter,
  trackVolumeStageTop,
} from "./trackVolume";
import { TrackVolumeStage } from "./TrackVolumeStage";

interface TrackVolumeFaderProps {
  readonly track: GridTrack;
  readonly onSetTrackVolume:
    ((index: number, step: number) => void) | undefined;
}

export function TrackVolumeFader({
  track,
  onSetTrackVolume,
}: TrackVolumeFaderProps): ReactElement {
  const inactive = !track.hasInstrument;
  const current = track.volumeStep;
  const capTop = inactive ? 0 : trackVolumeStageTop(current);
  const over = !inactive && current === 4;
  const readoutDb = `${track.volumeDb.toFixed(1)} dB`;
  const stages = TRACK_GRID_ROWS.filter((row) => row.role === "volume");

  return (
    <div
      className={`track-grid__volume${inactive ? " track-grid__volume--inactive" : ""}${over ? " track-grid__volume--over" : ""}`}
      data-testid="track-fader"
      data-track-index={track.index}
      data-volume-step={track.volumeStep}
      data-db={track.volumeDb.toFixed(1)}
      data-inactive={inactive ? "true" : "false"}
    >
      <span
        className="track-grid__fader-readout"
        data-testid="track-fader-readout"
        data-track-index={track.index}
        data-volume-step={track.volumeStep}
        data-db={track.volumeDb.toFixed(1)}
      >
        <span className="track-grid__fader-db">{readoutDb}</span>
      </span>

      <div className="track-grid__fader-visual" aria-hidden="true">
        <div className="track-grid__fader-track">
          <span
            className={`track-grid__fader-cap${over ? " track-grid__fader-cap--over" : ""}${current === 0 ? " track-grid__fader-cap--bottom" : ""}`}
            data-testid="track-fader-cap"
            data-step={track.volumeStep}
            style={{ bottom: `${String(capTop)}%` }}
          />

          <div className="track-grid__fader-scale">
            {TRACK_VOLUME_STEPS.map((stage) => {
              const position = trackVolumeStageCenter(stage.step);
              const active = !inactive && stage.step === current;
              const reached = !inactive && stage.step <= current;
              const stageOver = stage.step === 4;

              return (
                <span
                  key={stage.step}
                  className={`track-grid__fader-tick${reached ? " track-grid__fader-tick--reached" : ""}${active ? " track-grid__fader-tick--active" : ""}${stageOver ? " track-grid__fader-tick--over" : ""}`}
                  data-testid="track-fader-tick"
                  data-stage={stage.step}
                  data-position={position}
                  data-label={stage.label}
                  data-active={active ? "true" : "false"}
                  data-reached={reached ? "true" : "false"}
                  data-over={stageOver ? "true" : "false"}
                  style={{ bottom: `${String(position)}%` }}
                >
                  <span className="track-grid__fader-tick-line" />
                  <span className="track-grid__fader-label">{stage.label}</span>
                </span>
              );
            })}
          </div>
        </div>
      </div>

      <div className="track-grid__fader-stages">
        {stages.map((row) => (
          <TrackVolumeStage
            key={`volume-${String(row.step ?? 0)}`}
            track={track}
            step={row.step ?? 0}
            onSetTrackVolume={onSetTrackVolume}
          />
        ))}
      </div>

      <TrackLevelMeter
        slot={track.slot}
        index={track.index}
        active={!inactive}
      />
    </div>
  );
}
