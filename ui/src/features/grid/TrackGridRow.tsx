import type { ReactElement } from "react";

import type { GridModel } from "./model";
import type { TrackGridRow as TrackGridRowSpec } from "./trackModel";
import { TrackFxToggle } from "./TrackFxToggle";
import type { TrackGridActions } from "./TrackGridTable";
import { TrackSelectCell } from "./TrackSelectCell";
import { TrackVolumeFader } from "./TrackVolumeFader";

export interface TrackGridRowProps extends TrackGridActions {
  readonly row: TrackGridRowSpec;
  readonly model: GridModel;
}

export function TrackGridRow({
  row,
  model,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
}: TrackGridRowProps): ReactElement {
  return (
    <div
      className="grid__row"
      role="row"
      data-testid="track-row"
      data-row={row.row}
      data-role={row.role}
      data-hot={row.hot ?? ""}
      data-step={row.step ?? ""}
    >
      {model.tracks.map((track) => {
        if (row.role === "track")
          return (
            <TrackSelectCell
              key={`${track.id}-${String(row.row)}`}
              track={track}
              armedSlot={model.armedSlot}
              onSelectTrack={onSelectTrack}
              onOpenTrack={onOpenTrack}
            />
          );

        if (row.role === "fx")
          return (
            <TrackFxToggle
              key={`${track.id}-${String(row.row)}`}
              track={track}
              hot={row.hot ?? 0}
              onToggleTrackFx={onToggleTrackFx}
            />
          );

        return (
          <TrackVolumeFader
            key={`${track.id}-${String(row.row)}`}
            track={track}
            step={row.step ?? 0}
            onSetTrackVolume={onSetTrackVolume}
          />
        );
      })}
    </div>
  );
}
