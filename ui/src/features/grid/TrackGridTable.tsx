import type { ReactElement } from "react";

import type { GridModel } from "./model";
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
        aria-rowcount={TRACK_GRID_ROWS.length}
        aria-colcount={model.tracks.length}
        data-testid="track-grid-table"
      >
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
