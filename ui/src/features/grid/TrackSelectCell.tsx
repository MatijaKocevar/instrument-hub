import type { ReactElement } from "react";

import { EMPTY_COLOUR, trackColourForIndex } from "./colours";
import { pad2 } from "./format";
import type { GridTrack } from "./trackModel";

interface TrackSelectCellProps {
  readonly track: GridTrack;
  readonly armedSlot: number;
  readonly onSelectTrack: ((index: number) => void) | undefined;
  readonly onOpenTrack: ((index: number) => void) | undefined;
}

export function TrackSelectCell({
  track,
  armedSlot,
  onSelectTrack,
  onOpenTrack,
}: TrackSelectCellProps): ReactElement {
  const selected = track.selected || track.slot === armedSlot;
  const colour = trackColourForIndex(track.index);
  const empty = !track.hasInstrument;

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
      data-colour-index={track.colorIndex}
      style={{
        background: empty
          ? `linear-gradient(150deg, ${EMPTY_COLOUR}33 0%, #1c1d28 70%)`
          : `linear-gradient(150deg, ${colour}88 0%, #1c1d28 70%)`,
      }}
    >
      <button
        className="grid__cell-main track-grid__selector-button"
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
        <span className="track-grid__selector-head">
          <span className="grid__cell-slot">T{pad2(track.index + 1)}</span>
          <span
            className={`track-grid__selector-state${selected ? " track-grid__selector-state--selected" : ""}`}
          >
            {selected ? "Selected" : empty ? "Empty" : "Ready"}
          </span>
        </span>
        <span className="grid__cell-name">{track.name}</span>
        <span className="track-grid__sub">
          {empty
            ? "No instrument"
            : track.instrument.name.length > 0
              ? track.instrument.name
              : "No instrument"}
        </span>
      </button>
    </div>
  );
}
