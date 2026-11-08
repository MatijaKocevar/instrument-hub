import type { ReactElement } from "react";

import type { ControlPatch, ControlTrack } from "../../api/client";
import { InlineRenameInput, useInlineRename } from "../rename";
import { latencyReadout } from "./latencyReadout";

interface InstrumentHeadProps {
  readonly patch: ControlPatch;
  readonly armed: boolean;
  readonly following: boolean;
  readonly track: ControlTrack | null;
  readonly onRenameTrack?: (index: number, name: string) => void;
}

export function InstrumentHead({
  patch,
  armed,
  following,
  track,
  onRenameTrack,
}: InstrumentHeadProps): ReactElement {
  const name = track?.name ?? patch.name;
  const canRename = track !== null && onRenameTrack !== undefined;
  const rename = useInlineRename(name, (next) => {
    if (track !== null) onRenameTrack?.(track.index, next);
  });

  return (
    <header className="instrument__head">
      <p className="instrument__eyebrow">Instrument · {patch.familyName}</p>

      {rename.editing ? (
        <InlineRenameInput
          className="instrument__title-rename"
          inputClassName="instrument__title-input"
          testId="instrument-rename-input"
          value={rename.draft}
          ariaLabel={`Track name for ${name}`}
          onValueChange={rename.setDraft}
          onSave={rename.save}
          onCancel={rename.cancel}
        />
      ) : (
        <h2
          className={`instrument__title${canRename ? " instrument__title--editable" : ""}`}
          data-testid="instrument-name"
          title={canRename ? `${name} · double-click to rename` : name}
          onDoubleClick={
            canRename
              ? (event) => {
                  event.preventDefault();
                  rename.start();
                }
              : undefined
          }
        >
          {name}
        </h2>
      )}

      <p className="instrument__lede">
        {patch.familyName} · slot {String(patch.slot + 1).padStart(2, "0")}
        {armed ? " · armed" : " · not armed"}
        {following ? " · following the controller" : " · pinned selection"}
      </p>

      <div className="instrument__badges">
        <span
          className="instrument__badge instrument__badge--warm"
          data-testid="instrument-warm-state"
          data-warm={patch.warm ? "true" : "false"}
          data-warm-state={patch.warmState}
        >
          {patch.warmState}
        </span>
        <span
          className="instrument__badge instrument__badge--latency"
          data-testid="instrument-latency"
          data-latency-samples={patch.latencySamples}
          data-latency-ms={patch.latencyMs.toFixed(3)}
        >
          {latencyReadout(patch.latencySamples, patch.latencyMs)}
        </span>
      </div>
    </header>
  );
}
