import type { ReactElement } from "react";

import { RecoveringBanner } from "./RecoveringBanner";
import type { GridModel } from "./model";
import { TrackGridTable } from "./TrackGridTable";
import "./grid.css";

export interface GridViewProps {
  readonly model: GridModel;
  readonly loading?: boolean;
  readonly onSelectTrack?: (index: number) => void;
  readonly onOpenTrack?: (index: number) => void;
  readonly onToggleTrackFx?: (index: number, hot: number) => void;
  readonly onSetTrackVolume?: (index: number, step: number) => void;
  readonly onRenameTrack?: (index: number, name: string) => void;
}

export function GridView({
  model,
  loading = false,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
  onRenameTrack,
}: GridViewProps): ReactElement {
  return (
    <section
      className="grid"
      aria-label="Instrument grid"
      data-testid="instrument-grid"
      data-layout={model.layout}
      data-armed-slot={model.armedSlot}
      data-loading={loading ? "true" : "false"}
    >
      {model.pluginPage !== null ? (
        <RecoveringBanner page={model.pluginPage} />
      ) : null}

      {loading ? (
        <div className="grid__empty" role="status" aria-busy="true">
          Loading tracks…
        </div>
      ) : model.tracks.length === 0 ? (
        <div className="grid__empty" data-testid="grid-empty">
          No performance tracks in this set yet.
        </div>
      ) : (
        <TrackGridTable
          model={model}
          {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
          {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
          {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
          {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
          {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
        />
      )}
    </section>
  );
}
