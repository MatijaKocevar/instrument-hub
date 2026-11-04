import type { ReactElement } from "react";

import { ARMED_COLOUR, colourForIndex } from "./colours";
import { pad2 } from "./format";
import type { GridModel } from "./model";
import type { GridTrack, TrackGridRow as TrackGridRowSpec } from "./trackModel";
import { volumeStepForRow } from "./trackVolume";
import type { TrackGridActions } from "./TrackGridTable";

function TrackSelectCell({
  track,
  armedSlot,
  onSelectTrack,
  onOpenTrack,
}: {
  readonly track: GridTrack;
  readonly armedSlot: number;
  readonly onSelectTrack: ((index: number) => void) | undefined;
  readonly onOpenTrack: ((index: number) => void) | undefined;
}): ReactElement {
  const selected = track.selected || track.slot === armedSlot;
  const colour = colourForIndex(track.colorIndex);

  return (
    <div
      className={`grid__cell track-grid__cell track-grid__cell--track${selected ? " grid__cell--armed" : ""}`}
      role="gridcell"
      data-testid="track-cell"
      data-track-index={track.index}
      data-track-id={track.id}
      data-row={7}
      data-role="track"
      data-slot={track.slot}
      data-selected={selected ? "true" : "false"}
      data-colour-index={track.colorIndex}
      style={{
        background: selected
          ? ARMED_COLOUR
          : `linear-gradient(150deg, ${colour}55 0%, #1c1d28 70%)`,
      }}
    >
      <button
        className="grid__cell-main"
        type="button"
        data-testid="track-select"
        data-track-index={track.index}
        data-slot={track.slot}
        aria-pressed={selected}
        onClick={
          onSelectTrack === undefined
            ? undefined
            : () => {
                onSelectTrack(track.index);
              }
        }
        onDoubleClick={
          onOpenTrack === undefined
            ? undefined
            : () => {
                onOpenTrack(track.index);
              }
        }
      >
        <span className="grid__cell-name">{track.name}</span>
        <span className="track-grid__sub">
          {track.instrument.name.length > 0
            ? track.instrument.name
            : "No instrument"}
        </span>
        <span className="grid__cell-foot">
          <span className="grid__cell-slot">T{pad2(track.index + 1)}</span>
          {selected ? <span className="grid__cell-badge">Selected</span> : null}
        </span>
      </button>
    </div>
  );
}

function HotFxCell({
  track,
  hot,
  onToggleTrackFx,
}: {
  readonly track: GridTrack;
  readonly hot: number;
  readonly onToggleTrackFx: ((index: number, hot: number) => void) | undefined;
}): ReactElement {
  const entry = track.hotFx[hot];
  const assigned = entry !== undefined && entry.id.length > 0;
  const enabled = assigned && entry?.bypassed !== true;
  const label = assigned ? ((entry?.name || entry?.id) ?? "") : "Unassigned";

  return (
    <div
      className={`grid__cell track-grid__cell track-grid__cell--fx${assigned ? " track-grid__cell--assigned" : ""}${enabled ? " track-grid__cell--enabled" : ""}`}
      role="gridcell"
      data-testid="track-cell"
      data-track-index={track.index}
      data-row={hot === 0 ? 6 : 5}
      data-role="fx"
      data-hot={hot}
      data-assigned={assigned ? "true" : "false"}
      data-bypassed={entry?.bypassed === true ? "true" : "false"}
      data-name={entry?.name ?? ""}
    >
      <button
        className="grid__cell-main track-grid__fx-button"
        type="button"
        data-testid="track-hot-fx"
        data-track-index={track.index}
        data-hot={hot}
        data-assigned={assigned ? "true" : "false"}
        data-bypassed={entry?.bypassed === true ? "true" : "false"}
        aria-pressed={enabled}
        aria-label={`Track ${String(track.index + 1)} hot FX ${String(hot + 1)}: ${label}`}
        disabled={!assigned || onToggleTrackFx === undefined}
        onClick={
          onToggleTrackFx === undefined
            ? undefined
            : () => {
                onToggleTrackFx(track.index, hot);
              }
        }
      >
        <span className="grid__cell-name">{label}</span>
        <span className="track-grid__sub">
          {assigned ? (enabled ? "On" : "Bypassed") : "Assign in Instrument"}
        </span>
        <span className="grid__cell-foot">
          <span className="grid__cell-slot">FX {String(hot + 1)}</span>
          {assigned ? (
            <span className="track-grid__state">{enabled ? "on" : "off"}</span>
          ) : null}
        </span>
      </button>
    </div>
  );
}

function VolumeCell({
  track,
  step,
  onSetTrackVolume,
}: {
  readonly track: GridTrack;
  readonly step: number;
  readonly onSetTrackVolume:
    ((index: number, step: number) => void) | undefined;
}): ReactElement {
  const active =
    step === 4
      ? track.volumeStep === 4 || track.volumeStep === 5
      : track.volumeStep === step;
  const over = step === 4 && track.volumeStep === 5;
  const label = over ? "+6 dB" : step === 0 ? "0%" : `${String(step * 25)}%`;

  return (
    <div
      className={`grid__cell track-grid__cell track-grid__cell--volume${active ? " track-grid__cell--volume-active" : ""}${over ? " track-grid__cell--over" : ""}`}
      role="gridcell"
      data-testid="track-cell"
      data-track-index={track.index}
      data-row={step}
      data-role="volume"
      data-step={step}
      data-active={active ? "true" : "false"}
      data-over={over ? "true" : "false"}
      data-volume-step={track.volumeStep}
    >
      <button
        className="grid__cell-main"
        type="button"
        data-testid="track-volume"
        data-track-index={track.index}
        data-step={step}
        data-active={active ? "true" : "false"}
        data-over={over ? "true" : "false"}
        aria-pressed={active}
        aria-label={`Track ${String(track.index + 1)} volume ${label}`}
        onClick={
          onSetTrackVolume === undefined
            ? undefined
            : () => {
                onSetTrackVolume(
                  track.index,
                  volumeStepForRow(step, track.volumeStep),
                );
              }
        }
      >
        <span className="grid__cell-name">{label}</span>
        <span className="track-grid__sub">
          {over
            ? "over"
            : active
              ? `${track.volumeDb.toFixed(1)} dB`
              : "set level"}
        </span>
        <span className="grid__cell-foot">
          <span className="grid__cell-slot">VOL {String(step + 1)}</span>
        </span>
      </button>
    </div>
  );
}

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
      <div className="grid__family track-grid__row-head" role="rowheader">
        <span className="grid__family-index">{pad2(row.row + 1)}</span>
        <span className="grid__family-text">{row.label}</span>
      </div>

      {model.tracks.map((track) => (
        <TrackGridCell
          key={`${track.id}-${String(row.row)}`}
          track={track}
          row={row}
          armedSlot={model.armedSlot}
          {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
          {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
          {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
          {...(onSetTrackVolume !== undefined ? { onSetTrackVolume } : {})}
        />
      ))}
    </div>
  );
}

function TrackGridCell({
  track,
  row,
  armedSlot,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
}: TrackGridActions & {
  readonly track: GridTrack;
  readonly row: TrackGridRowSpec;
  readonly armedSlot: number;
}): ReactElement {
  if (row.role === "track")
    return (
      <TrackSelectCell
        track={track}
        armedSlot={armedSlot}
        onSelectTrack={onSelectTrack}
        onOpenTrack={onOpenTrack}
      />
    );

  if (row.role === "fx")
    return (
      <HotFxCell
        track={track}
        hot={row.hot ?? 0}
        onToggleTrackFx={onToggleTrackFx}
      />
    );

  return (
    <VolumeCell
      track={track}
      step={row.step ?? 0}
      onSetTrackVolume={onSetTrackVolume}
    />
  );
}
