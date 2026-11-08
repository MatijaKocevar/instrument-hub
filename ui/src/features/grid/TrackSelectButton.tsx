import type { ReactElement } from "react";

import type { GridTrack } from "./trackModel";

export interface TrackSelectButtonProps {
  readonly track: GridTrack;
  readonly onSelectTrack: ((index: number) => void) | undefined;
  readonly onOpenTrack: ((index: number) => void) | undefined;
  readonly onStartRename?: () => void;
}

export function TrackSelectButton({
  track,
  onSelectTrack,
  onOpenTrack,
  onStartRename,
}: TrackSelectButtonProps): ReactElement {
  const empty = !track.hasInstrument;
  const instrument =
    empty || track.instrument.name.length === 0
      ? "No instrument"
      : track.instrument.name;

  return (
    <button
      className="grid__cell-main track-grid__selector-button"
      type="button"
      data-testid="track-select"
      data-track-index={track.index}
      data-slot={track.slot}
      aria-pressed={track.selected}
      title={`${track.name} · double-click to open`}
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
      <span
        className={`grid__cell-name${onStartRename === undefined ? "" : " grid__cell-name--editable"}`}
        data-testid="track-name"
        title={
          onStartRename === undefined
            ? track.name
            : `${track.name} · double-click to rename`
        }
        onDoubleClick={
          onStartRename === undefined
            ? undefined
            : (event) => {
                event.preventDefault();
                event.stopPropagation();
                onStartRename();
              }
        }
      >
        {track.name}
      </span>
      <span
        className="track-grid__sub"
        data-testid="track-instrument"
        data-instrument-kind={track.instrument.kind}
      >
        {instrument}
      </span>
    </button>
  );
}
