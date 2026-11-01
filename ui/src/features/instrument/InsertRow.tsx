import type { ReactElement } from "react";

import type { ControlFxInsert, ControlPatch } from "../../api/client";
import { wetPercent } from "./format";
import { insertLabel } from "./inserts";

interface InsertRowProps {
  readonly patch: ControlPatch;
  readonly insert: ControlFxInsert;
  readonly index: number;
  readonly count: number;
  readonly onRemoveInsert?: (slot: number, index: number) => void;
  readonly onToggleBypass?: (
    slot: number,
    index: number,
    bypassed: boolean,
  ) => void;
  readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
}

export function InsertRow({
  patch,
  insert,
  index,
  count,
  onRemoveInsert,
  onToggleBypass,
  onMoveInsert,
}: InsertRowProps): ReactElement {
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
