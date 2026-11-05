import type { ReactElement } from "react";

import { GridHeader } from "./GridHeader";
import { GridTable } from "./GridTable";
import { LoadingGrid } from "./LoadingGrid";
import { RecoveringBanner } from "./RecoveringBanner";
import type { GridModel } from "./model";
import { EMPTY_METERS, type MeterSnapshot } from "./meterFrame";
import { TrackGridTable } from "./TrackGridTable";
import "./grid.css";

export interface GridViewProps {
  readonly model: GridModel;
  readonly meters?: MeterSnapshot;
  readonly loading?: boolean;
  readonly onArm?: (slot: number) => void;
  readonly onColourChange?: (slot: number, colorIndex: number) => void;
  readonly onFamilyColourChange?: (
    familyId: string,
    slots: readonly number[],
    colorIndex: number,
  ) => void;
  readonly onSelectTrack?: (index: number) => void;
  readonly onOpenTrack?: (index: number) => void;
  readonly onToggleTrackFx?: (index: number, hot: number) => void;
  readonly onSetTrackVolume?: (index: number, step: number) => void;
  readonly onRenameTrack?: (index: number, name: string) => void;
}

export function GridView({
  model,
  meters = EMPTY_METERS,
  loading = false,
  onArm,
  onColourChange,
  onFamilyColourChange,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
  onRenameTrack,
}: GridViewProps): ReactElement {
  const empty =
    model.layout === "tracks"
      ? model.tracks.length === 0
      : model.families.length === 0;

  return (
    <section
      className="grid"
      aria-label="Instrument grid"
      data-testid="instrument-grid"
      data-layout={model.layout}
      data-armed-slot={model.armedSlot}
      data-loading={loading ? "true" : "false"}
    >
      {model.layout === "tracks" ? null : (
        <GridHeader meters={meters} layout={model.layout} />
      )}

      {model.pluginPage !== null ? (
        <RecoveringBanner page={model.pluginPage} />
      ) : null}

      {loading ? (
        <LoadingGrid />
      ) : empty ? (
        <div className="grid__empty" data-testid="grid-empty">
          {model.layout === "tracks"
            ? "No performance tracks in this set yet."
            : "No instruments in this set yet."}
        </div>
      ) : model.layout === "tracks" ? (
        <TrackGridTable
          model={model}
          {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
          {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
          {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
          {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
          {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
        />
      ) : (
        <GridTable
          model={model}
          meters={meters}
          onArm={onArm}
          onColourChange={onColourChange}
          onFamilyColourChange={onFamilyColourChange}
        />
      )}
    </section>
  );
}
