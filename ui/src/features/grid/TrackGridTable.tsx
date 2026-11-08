import type { ReactElement } from "react";

import type { GridModel } from "./model";
import { TrackGridRow } from "./TrackGridRow";
import "./trackGrid.css";

export interface TrackGridActions {
  readonly onSelectTrack?: (index: number) => void;
  readonly onOpenTrack?: (index: number) => void;
  readonly onToggleTrackFx?: (index: number, hot: number) => void;
  readonly onSetTrackVolume?: (index: number, gain: number) => void;
  readonly onRenameTrack?: (index: number, name: string) => void;
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
  onRenameTrack,
}: TrackGridTableProps): ReactElement {
  return (
    <div className="grid__scroll">
      <div
        className="track-grid__table"
        role="grid"
        aria-label="Performance tracks and rows"
        aria-rowcount={model.tracks.length}
        aria-colcount={8}
        data-testid="track-grid-table"
      >
        {model.tracks.map((track) => (
          <TrackGridRow
            key={track.id}
            track={track}
            {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
            {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
            {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
            {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
            {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
          />
        ))}
      </div>
    </div>
  );
}
