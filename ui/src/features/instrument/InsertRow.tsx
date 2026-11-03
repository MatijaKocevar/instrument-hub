import type { ReactElement } from "react";

import type {
  ControlFxInsert,
  ControlPatch,
  ControlTrack,
} from "../../api/client";
import { wetPercent } from "./format";
import { insertLabel } from "./inserts";

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
  onAssignHotFx,
}: InsertRowProps): ReactElement {
  const hotFor = (hot: number) =>
    track?.hotFx.find((candidate) => candidate.index === hot) ?? null;

  return (
    <li
      className={`instrument__insert${insert.bypassed ? " instrument__insert--bypassed" : ""}`}
      data-testid="fx-insert"
      data-index={index}
      data-type={insert.type}
      data-kind={insert.kind}
      data-name={insert.name}
      data-bypassed={insert.bypassed ? "true" : "false"}
      data-wet={insert.wet.toFixed(2)}
      data-latency-samples={insert.latencySamples}
    >
      <span className="instrument__insert-index">
        {String(index + 1).padStart(2, "0")}
      </span>
      <span className="instrument__insert-name">
        <span className="instrument__insert-type">
          {insertLabel(insert.type)}
        </span>
        <span className="instrument__insert-id">{insert.name}</span>
      </span>
      <span className="instrument__insert-wet" data-testid="fx-wet">
        {String(wetPercent(insert.wet))}% wet
      </span>
      <span className="instrument__insert-latency">
        {String(insert.bypassed ? 0 : insert.latencySamples)} smp
      </span>

      <span className="instrument__insert-actions">
        {track === null
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
          title={`Move ${insert.name} up`}
          aria-label={`Move ${insert.name} up`}
          data-testid="fx-move-up"
          disabled={index === 0}
          onClick={() => {
            onMoveInsert?.(patch.slot, index, index - 1);
          }}
        >
          ↑
        </button>
        <button
          className="instrument__icon-button"
          type="button"
          title={`Move ${insert.name} down`}
          aria-label={`Move ${insert.name} down`}
          data-testid="fx-move-down"
          disabled={index === count - 1}
          onClick={() => {
            onMoveInsert?.(patch.slot, index, index + 1);
          }}
        >
          ↓
        </button>
        <button
          className={`instrument__toggle instrument__toggle--small${insert.bypassed ? "" : " instrument__toggle--on"}`}
          type="button"
          aria-pressed={!insert.bypassed}
          title={
            insert.bypassed ? `Enable ${insert.name}` : `Bypass ${insert.name}`
          }
          data-testid="fx-bypass"
          onClick={() => {
            onToggleBypass?.(patch.slot, index, !insert.bypassed);
          }}
        >
          {insert.bypassed ? "Off" : "On"}
        </button>
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
      </span>
    </li>
  );
}
