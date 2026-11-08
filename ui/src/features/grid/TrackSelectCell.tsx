import type { ReactElement } from "react";

import { InlineRenameInput, useInlineRename } from "../rename";
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
  const rename = useInlineRename(track.name, (name) => {
    onRenameTrack?.(track.index, name);
  });
  const selected = track.selected;
  const colour = trackColourForIndex(track.index);
  const empty = !track.hasInstrument;
  const statusLabel = STATUS_LABELS[track.status];
  const canRename = onRenameTrack !== undefined;

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
        {canRename ? (
          <button
            className="track-grid__rename-button"
            type="button"
            data-testid="track-rename"
            data-track-index={track.index}
            title={`Rename ${track.name}`}
            aria-label={`Rename ${track.name}`}
            onClick={rename.start}
          >
            ✎
          </button>
        ) : null}
      </span>

      {rename.editing ? (
        <InlineRenameInput
          className="track-grid__rename"
          inputClassName="track-grid__rename-input"
          testId="track-rename-input"
          value={rename.draft}
          ariaLabel={`Track name for ${track.name}`}
          onValueChange={rename.setDraft}
          onSave={rename.save}
          onCancel={rename.cancel}
        />
      ) : (
        <TrackSelectButton
          track={track}
          onSelectTrack={onSelectTrack}
          onOpenTrack={onOpenTrack}
          {...(canRename ? { onStartRename: rename.start } : {})}
        />
      )}
    </div>
  );
}
