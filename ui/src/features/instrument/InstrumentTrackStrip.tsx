import type { ReactElement } from "react";

import type { ControlPatch } from "../../api/client";
import {
  TrackFxToggle,
  TrackMeterProvider,
  TrackSelectCell,
  TrackVolumeFader,
  type GridTrack,
  type TrackGridActions,
} from "../grid";
import "../grid/grid.css";
import "../grid/trackGrid.css";
import { LiveTrackChannel } from "./LiveTrackChannel";

export interface InstrumentTrackStripProps extends TrackGridActions {
  readonly track: GridTrack;
  readonly patch: ControlPatch;
  readonly metersUrl: string;
  readonly onPanChange?: (slot: number, pan: number) => void;
  readonly onMuteChange?: (slot: number, muted: boolean) => void;
  readonly onSoloChange?: (slot: number, solo: boolean) => void;
}

export function InstrumentTrackStrip({
  track,
  patch,
  metersUrl,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
  onRenameTrack,
  onPanChange,
  onMuteChange,
  onSoloChange,
}: InstrumentTrackStripProps): ReactElement {
  const column = (
    <section
      className={`track-grid__column instrument__live-track${track.selected ? " track-grid__column--selected" : ""}`}
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

      <TrackFxToggle track={track} hot={0} onToggleTrackFx={onToggleTrackFx} />
      <TrackFxToggle track={track} hot={1} onToggleTrackFx={onToggleTrackFx} />

      <LiveTrackChannel
        patch={patch}
        {...(onPanChange !== undefined ? { onPanChange } : {})}
        {...(onMuteChange !== undefined ? { onMuteChange } : {})}
        {...(onSoloChange !== undefined ? { onSoloChange } : {})}
      />

      <TrackVolumeFader track={track} onSetTrackVolume={onSetTrackVolume} />
    </section>
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
        aria-colcount={5}
      >
        {metersUrl.length > 0 ? (
          <TrackMeterProvider url={metersUrl}>{column}</TrackMeterProvider>
        ) : (
          column
        )}
      </div>
    </aside>
  );
}
