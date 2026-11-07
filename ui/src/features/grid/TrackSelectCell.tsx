import { useRef, useState, type ReactElement } from "react";

import { EMPTY_COLOUR, trackColourForIndex } from "./colours";
import { pad2 } from "./format";
import type { GridTrack, TrackStatus } from "./trackModel";
import { TrackSelectButton } from "./TrackSelectButton";

interface TrackSelectCellProps {
  readonly track: GridTrack;
  readonly onSelectTrack: ((index: number) => void) | undefined;
  readonly onOpenTrack: ((index: number) => void) | undefined;
  readonly onRenameTrack: ((index: number, name: string) => void) | undefined;
}

const STATUS_LABELS: Record<TrackStatus, string> = {
  ok: "Ready",
  warming: "Warming",
  error: "Error",
  empty: "No instrument",
};

export function TrackSelectCell({
  track,
  onSelectTrack,
  onOpenTrack,
  onRenameTrack,
}: TrackSelectCellProps): ReactElement {
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState(track.name);
  const finished = useRef(false);
  const selected = track.selected;
  const colour = trackColourForIndex(track.index);
  const empty = !track.hasInstrument;
  const statusLabel = STATUS_LABELS[track.status];

  const startRename = (): void => {
    finished.current = false;
    setDraft(track.name);
    setEditing(true);
  };

  const finishRename = (save: boolean): void => {
    if (finished.current) return;

    finished.current = true;
    const next = draft.trim();

    if (save && next.length > 0 && next !== track.name)
      onRenameTrack?.(track.index, next);

    setEditing(false);
  };

  return (
    <div
      className={`grid__cell track-grid__cell track-grid__cell--track track-grid__selector${selected ? " track-grid__selector--selected" : ""}${!selected && empty ? " track-grid__selector--empty" : ""}`}
      role="gridcell"
      data-testid="track-cell"
      data-track-index={track.index}
      data-track-id={track.id}
      data-row={7}
      data-role="track"
      data-slot={track.slot}
      data-selected={selected ? "true" : "false"}
      data-status={track.status}
      data-colour-index={track.colorIndex}
      style={{
        background: empty
          ? `linear-gradient(150deg, ${EMPTY_COLOUR}33 0%, #1c1d28 70%)`
          : `linear-gradient(150deg, ${colour}88 0%, #1c1d28 70%)`,
      }}
    >
      <span className="track-grid__selector-top">
        <span
          className="track-grid__status"
          data-testid="track-status"
          data-status={track.status}
          role="img"
          aria-label={`Track status: ${statusLabel}`}
          title={statusLabel}
        />
        <span className="grid__cell-slot">T{pad2(track.index + 1)}</span>
        <span
          className={`track-grid__selector-state${selected ? " track-grid__selector-state--selected" : ""}`}
          data-testid="track-selection"
          data-selected={selected ? "true" : "false"}
        >
          {selected ? "Selected" : empty ? "Empty" : "Ready"}
        </span>
        {onRenameTrack === undefined ? null : (
          <button
            className="track-grid__rename-button"
            type="button"
            data-testid="track-rename"
            data-track-index={track.index}
            title={`Rename ${track.name}`}
            aria-label={`Rename ${track.name}`}
            onClick={startRename}
          >
            ✎
          </button>
        )}
      </span>

      {editing ? (
        <form
          className="track-grid__rename"
          onSubmit={(event) => {
            event.preventDefault();
            finishRename(true);
          }}
          onKeyDown={(event) => {
            if (event.key !== "Escape") return;

            event.preventDefault();
            finishRename(false);
          }}
        >
          <input
            className="track-grid__rename-input"
            data-testid="track-rename-input"
            data-track-index={track.index}
            value={draft}
            maxLength={80}
            autoFocus
            aria-label={`Track name for ${track.name}`}
            onChange={(event) => {
              setDraft(event.target.value);
            }}
            onBlur={() => {
              finishRename(true);
            }}
          />
        </form>
      ) : (
        <TrackSelectButton
          track={track}
          onSelectTrack={onSelectTrack}
          onOpenTrack={onOpenTrack}
        />
      )}
    </div>
  );
}
