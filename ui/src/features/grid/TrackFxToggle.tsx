import type { ReactElement } from "react";

import type { GridTrack } from "./trackModel";

interface TrackFxToggleProps {
  readonly track: GridTrack;
  readonly hot: number;
  readonly onToggleTrackFx: ((index: number, hot: number) => void) | undefined;
}

export function TrackFxToggle({
  track,
  hot,
  onToggleTrackFx,
}: TrackFxToggleProps): ReactElement {
  const inactive = !track.hasInstrument;
  const entry = track.hotFx[hot];
  const assigned = !inactive && entry !== undefined && entry.id.length > 0;
  const enabled = assigned && entry?.bypassed !== true;
  const label = inactive
    ? "No instrument"
    : assigned
      ? ((entry?.name || entry?.id) ?? "")
      : "Unassigned";

  return (
    <div
      className={`grid__cell track-grid__cell track-grid__cell--fx track-grid__fx-toggle${inactive ? " track-grid__cell--inactive" : ""}${assigned ? " track-grid__cell--assigned" : " track-grid__fx-toggle--unassigned"}${enabled ? " track-grid__cell--enabled" : ""}${assigned && !enabled ? " track-grid__fx-toggle--bypassed" : ""}`}
      role="gridcell"
      data-testid="track-cell"
      data-track-index={track.index}
      data-row={hot === 0 ? 6 : 5}
      data-role="fx"
      data-hot={hot}
      data-assigned={assigned ? "true" : "false"}
      data-bypassed={entry?.bypassed === true ? "true" : "false"}
      data-name={entry?.name ?? ""}
    >
      <button
        className="grid__cell-main track-grid__fx-button"
        type="button"
        data-testid="track-hot-fx"
        data-track-index={track.index}
        data-hot={hot}
        data-assigned={assigned ? "true" : "false"}
        data-bypassed={entry?.bypassed === true ? "true" : "false"}
        aria-pressed={enabled}
        aria-label={`Track ${String(track.index + 1)} hot FX ${String(hot + 1)}: ${label}`}
        disabled={inactive || !assigned || onToggleTrackFx === undefined}
        onClick={
          onToggleTrackFx === undefined
            ? undefined
            : () => {
                onToggleTrackFx(track.index, hot);
              }
        }
      >
        <span className="track-grid__fx-head">
          <span className="grid__cell-slot">FX {String(hot + 1)}</span>
          <span
            className={`track-grid__switch${enabled ? " track-grid__switch--on" : ""}${assigned ? "" : " track-grid__switch--empty"}${assigned && !enabled ? " track-grid__switch--bypass" : ""}`}
            data-testid="track-fx-switch"
            data-on={enabled ? "true" : "false"}
            data-assigned={assigned ? "true" : "false"}
            aria-hidden="true"
          >
            <span className="track-grid__switch-thumb" />
          </span>
        </span>
        <span className="grid__cell-name">{label}</span>
        <span
          className="track-grid__sub track-grid__fx-state"
          data-testid="track-fx-state"
          data-state={
            inactive
              ? "inactive"
              : assigned
                ? enabled
                  ? "on"
                  : "bypass"
                : "unassigned"
          }
        >
          {inactive
            ? "Assign an instrument"
            : assigned
              ? enabled
                ? "On"
                : "Bypassed"
              : "Assign in Instrument"}
        </span>
      </button>
    </div>
  );
}
