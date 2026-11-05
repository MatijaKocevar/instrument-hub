import type { ReactElement } from "react";

import type { GridTrack } from "./trackModel";
import { TRACK_GRID_ROWS } from "./trackModel";
import type { TrackGridActions } from "./TrackGridTable";
import { TrackFxToggle } from "./TrackFxToggle";
import { TrackSelectCell } from "./TrackSelectCell";
import { TrackVolumeFader } from "./TrackVolumeFader";

export interface TrackGridColumnProps extends TrackGridActions {
  readonly track: GridTrack;
}

export function TrackGridColumn({
  track,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
  onRenameTrack,
}: TrackGridColumnProps): ReactElement {
  return (
    <section
      className={`track-grid__column${track.selected ? " track-grid__column--selected" : ""}`}
      role="row"
      data-testid="track-column"
      data-track-index={track.index}
      data-track-id={track.id}
      data-selected={track.selected ? "true" : "false"}
      data-status={track.status}
    >
      <TrackSelectCell
        track={track}
        onSelectTrack={onSelectTrack}
        onOpenTrack={onOpenTrack}
        onRenameTrack={onRenameTrack}
      />

      {TRACK_GRID_ROWS.filter((row) => row.role === "fx").map((row) => (
        <TrackFxToggle
          key={`fx-${String(row.hot ?? 0)}`}
          track={track}
          hot={row.hot ?? 0}
          onToggleTrackFx={onToggleTrackFx}
        />
      ))}

      {TRACK_GRID_ROWS.filter((row) => row.role === "volume").map((row) => (
        <TrackVolumeFader
          key={`volume-${String(row.step ?? 0)}`}
          track={track}
          step={row.step ?? 0}
          onSetTrackVolume={onSetTrackVolume}
        />
      ))}
    </section>
  );
}
