import type { ReactElement } from "react";

import { pad2 } from "./format";
import { GRID_COLUMNS, type GridModel } from "./model";
import { TrackGridRow } from "./TrackGridRow";
import { TRACK_GRID_ROWS } from "./trackModel";
import "./trackGrid.css";

export interface TrackGridActions {
  readonly onSelectTrack?: (index: number) => void;
  readonly onOpenTrack?: (index: number) => void;
  readonly onToggleTrackFx?: (index: number, hot: number) => void;
  readonly onSetTrackVolume?: (index: number, step: number) => void;
}

export interface TrackGridTableProps extends TrackGridActions {
  readonly model: GridModel;
}

export function TrackGridTable({
  model,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
}: TrackGridTableProps): ReactElement {
  return (
    <div className="grid__scroll">
      <div
        className="grid__table track-grid__table"
        role="grid"
        aria-label="Performance tracks and rows"
        aria-rowcount={TRACK_GRID_ROWS.length + 1}
        aria-colcount={GRID_COLUMNS + 1}
        data-testid="track-grid-table"
      >
        <div className="grid__head" role="row" data-testid="track-grid-head">
          <span className="grid__corner" role="columnheader">
            Tracks
          </span>
          {model.tracks.map((track) => (
            <span
              className="grid__column"
              role="columnheader"
              key={track.id}
              data-testid="track-column"
              data-track-index={track.index}
              data-slot={track.slot}
            >
              {pad2(track.index + 1)}
            </span>
          ))}
        </div>

        {TRACK_GRID_ROWS.map((row) => (
          <TrackGridRow
            key={`row-${String(row.row)}`}
            row={row}
            model={model}
            {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
            {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
            {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
            {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
          />
        ))}
      </div>
    </div>
  );
}
