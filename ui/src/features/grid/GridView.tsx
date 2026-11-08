import type { ReactElement } from "react";

import { RecoveringBanner } from "./RecoveringBanner";
import type { GridModel } from "./model";
import { TrackGridSkeleton } from "./TrackGridSkeleton";
import { TrackGridTable } from "./TrackGridTable";
import { TrackMeterProvider } from "./TrackMeterProvider";
import "./grid.css";

export interface GridViewProps {
  readonly model: GridModel;
  readonly metersUrl?: string;
  readonly loading?: boolean;
  readonly onSelectTrack?: (index: number) => void;
  readonly onOpenTrack?: (index: number) => void;
  readonly onToggleTrackFx?: (index: number, hot: number) => void;
  readonly onSetTrackVolume?: (index: number, step: number) => void;
  readonly onRenameTrack?: (index: number, name: string) => void;
}

export function GridView({
  model,
  metersUrl = "",
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
      aria-busy={loading}
      data-testid="instrument-grid"
      data-layout={model.layout}
      data-armed-slot={model.armedSlot}
      data-loading={loading ? "true" : "false"}
    >
      {model.pluginPage !== null ? (
        <RecoveringBanner page={model.pluginPage} />
      ) : null}

      {loading ? (
        <TrackGridSkeleton />
      ) : model.tracks.length === 0 ? (
        <div className="grid__empty" data-testid="grid-empty">
          No performance tracks in this set yet.
        </div>
      ) : (
        <TrackMeterProvider url={metersUrl}>
          <TrackGridTable
            model={model}
            {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
            {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
            {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
            {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
            {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
          />
        </TrackMeterProvider>
      )}
    </section>
  );
}
