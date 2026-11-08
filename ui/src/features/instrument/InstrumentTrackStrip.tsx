import type { ReactElement } from "react";

import {
  TrackGridRow,
  TrackMeterProvider,
  type GridTrack,
  type TrackGridActions,
} from "../grid";
import "../grid/grid.css";
import "../grid/trackGrid.css";

export interface InstrumentTrackStripProps extends TrackGridActions {
  readonly track: GridTrack;
  readonly metersUrl: string;
}

export function InstrumentTrackStrip({
  track,
  metersUrl,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
  onRenameTrack,
}: InstrumentTrackStripProps): ReactElement {
  const row = (
    <TrackGridRow
      track={track}
      {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
      {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
      {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
      {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
      {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
    />
  );

  return (
    <aside
      className="instrument__strip"
      aria-label="Live track"
      data-testid="instrument-track-strip"
    >
      <div className="instrument__strip-head">
        <span className="instrument__strip-title">Live track</span>
        <span className="instrument__strip-slot">
          T{String(track.index + 1).padStart(2, "0")}
        </span>
      </div>
      <div
        className="instrument__strip-body"
        role="grid"
        aria-label="Live track controls"
        aria-rowcount={1}
        aria-colcount={8}
      >
        {metersUrl.length > 0 ? (
          <TrackMeterProvider url={metersUrl}>{row}</TrackMeterProvider>
        ) : (
          row
        )}
      </div>
    </aside>
  );
}
