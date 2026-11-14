import type { ReactElement } from "react";

import type {
  ControlFxInsert,
  ControlFxInsertUpdate,
  ControlFxTunerState,
  ControlPatch,
  ControlTrack,
} from "../../api/client";
import { DriveControls } from "./DriveControls";
import { wetPercent } from "./format";
import { insertLabel } from "./inserts";
import { TunerPanel } from "./TunerPanel";

interface InsertRowProps {
  readonly patch: ControlPatch;
  readonly insert: ControlFxInsert;
  readonly index: number;
  readonly count: number;
  readonly track?: ControlTrack | null;
  readonly onRemoveInsert?: (slot: number, index: number) => void;
  readonly onToggleBypass?: (
    slot: number,
    index: number,
    bypassed: boolean,
  ) => void;
  readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
  readonly onUpdateInsert?: (
    slot: number,
    index: number,
    update: ControlFxInsertUpdate,
  ) => void;
  readonly onReadTuner?: (
    slot: number,
    index: number,
  ) => Promise<ControlFxTunerState>;
  readonly active?: boolean;
  readonly onAssignHotFx?: (
    trackIndex: number,
    hot: number,
    insertId: string | null,
  ) => void;
}

export function InsertRow({
  patch,
  insert,
  index,
  count,
  track = null,
  onRemoveInsert,
  onToggleBypass,
  onMoveInsert,
  onUpdateInsert,
  onReadTuner,
  active = false,
  onAssignHotFx,
}: InsertRowProps): ReactElement {
  const tuner = insert.type === "tuner";
  const hotFor = (hot: number) =>
    track?.hotFx.find((candidate) => candidate.index === hot) ?? null;

  return (
    <li
      className={`instrument__insert${insert.bypassed && !tuner ? " instrument__insert--bypassed" : ""}`}
      data-testid="fx-insert"
      data-index={index}
      data-type={insert.type}
      data-kind={insert.kind}
      data-name={insert.name}
      data-bypassed={insert.bypassed ? "true" : "false"}
      data-wet={insert.wet.toFixed(2)}
      data-latency-samples={insert.latencySamples}
    >
      <div className="instrument__insert-head">
        <span className="instrument__insert-index">
          {String(index + 1).padStart(2, "0")}
        </span>
        <span className="instrument__insert-type">
          {insertLabel(insert.type)}
        </span>
        <span
          className={`instrument__insert-state${insert.bypassed && !tuner ? " instrument__insert-state--bypassed" : ""}`}
        >
          {tuner ? "Always on" : insert.bypassed ? "Bypassed" : "Active"}
        </span>
      </div>

      <span className="instrument__insert-name" data-testid="fx-insert-name">
        {insert.name}
      </span>

      <div className="instrument__insert-meta">
        <span className="instrument__insert-wet" data-testid="fx-wet">
          {tuner ? "pass-through" : `${String(wetPercent(insert.wet))}% wet`}
        </span>
        <span className="instrument__insert-latency">
          {String(insert.bypassed && !tuner ? 0 : insert.latencySamples)} smp
        </span>
      </div>

      {tuner ? (
        <TunerPanel
          slot={patch.slot}
          index={index}
          active={active}
          {...(onReadTuner !== undefined
            ? {
                read: (slot: number, row: number) => onReadTuner(slot, row),
              }
            : {})}
        />
      ) : insert.type === "drive" ? (
        <DriveControls
          insert={insert}
          {...(onUpdateInsert !== undefined
            ? {
                onUpdate: (update: ControlFxInsertUpdate) => {
                  onUpdateInsert(patch.slot, index, update);
                },
              }
            : {})}
        />
      ) : null}

      <div className="instrument__insert-actions">
        {track === null || tuner
          ? null
          : [0, 1].map((hot) => {
              const assigned = hotFor(hot)?.id === insert.id;

              return (
                <button
                  key={`hot-${String(hot)}`}
                  className={`instrument__hot-button${assigned ? " instrument__hot-button--on" : ""}`}
                  type="button"
                  title={
                    assigned
                      ? `Clear hot FX ${String(hot + 1)} from ${insert.name}`
                      : `Mark ${insert.name} as hot FX ${String(hot + 1)}`
                  }
                  aria-label={
                    assigned
                      ? `Clear hot FX ${String(hot + 1)}`
                      : `Mark hot FX ${String(hot + 1)}`
                  }
                  aria-pressed={assigned}
                  data-testid={`fx-hot-${String(hot + 1)}`}
                  data-hot={hot}
                  data-assigned={assigned ? "true" : "false"}
                  onClick={() => {
                    onAssignHotFx?.(
                      track.index,
                      hot,
                      assigned ? null : insert.id,
                    );
                  }}
                >
                  H{String(hot + 1)}
                </button>
              );
            })}
        <button
          className="instrument__icon-button"
          type="button"
          title={`Move ${insert.name} left`}
          aria-label={`Move ${insert.name} left`}
          data-testid="fx-move-up"
          disabled={index === 0}
          onClick={() => {
            onMoveInsert?.(patch.slot, index, index - 1);
          }}
        >
          ←
        </button>
        <button
          className="instrument__icon-button"
          type="button"
          title={`Move ${insert.name} right`}
          aria-label={`Move ${insert.name} right`}
          data-testid="fx-move-down"
          disabled={index === count - 1}
          onClick={() => {
            onMoveInsert?.(patch.slot, index, index + 1);
          }}
        >
          →
        </button>
        {tuner ? null : (
          <button
            className={`instrument__toggle instrument__toggle--small${insert.bypassed ? "" : " instrument__toggle--on"}`}
            type="button"
            aria-pressed={!insert.bypassed}
            title={
              insert.bypassed
                ? `Enable ${insert.name}`
                : `Bypass ${insert.name}`
            }
            data-testid="fx-bypass"
            onClick={() => {
              onToggleBypass?.(patch.slot, index, !insert.bypassed);
            }}
          >
            {insert.bypassed ? "Off" : "On"}
          </button>
        )}
        <button
          className="instrument__icon-button instrument__icon-button--remove"
          type="button"
          title={`Remove ${insert.name}`}
          aria-label={`Remove ${insert.name}`}
          data-testid="fx-remove"
          onClick={() => {
            onRemoveInsert?.(patch.slot, index);
          }}
        >
          ×
        </button>
      </div>
    </li>
  );
}
