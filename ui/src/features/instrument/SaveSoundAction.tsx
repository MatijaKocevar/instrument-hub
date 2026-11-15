import type { ReactElement } from "react";

import type { ControlPatch } from "../../api/client";
import { Spinner } from "../loading";
import { isPluginSource } from "./source";

interface SaveSoundActionProps {
  readonly patch: ControlPatch;
  readonly captureBusy?: boolean;
  readonly onCaptureState?: (slot: number) => void;
}

export function SaveSoundAction({
  patch,
  captureBusy,
  onCaptureState,
}: SaveSoundActionProps): ReactElement {
  const isPlugin = isPluginSource(patch);

  return (
    <div
      className="instrument__save"
      data-testid="patch-state-save"
      data-slot={patch.slot}
      data-enabled={isPlugin ? "true" : "false"}
      data-busy={captureBusy === true ? "true" : "false"}
    >
      <button
        className="instrument__button instrument__button--save"
        type="button"
        data-testid="source-save-state"
        data-slot={patch.slot}
        disabled={
          !isPlugin || captureBusy === true || onCaptureState === undefined
        }
        onClick={() => {
          onCaptureState?.(patch.slot);
        }}
      >
        {captureBusy === true ? (
          <Spinner label="Saving the running sound" />
        ) : null}
        {captureBusy === true ? "Saving…" : "Save sound to patch"}
      </button>

      <p className="instrument__field-hint" data-testid="source-save-hint">
        {isPlugin
          ? "Captures the running plug-in's current sound into this patch without switching away."
          : "Only plug-in sources have a sound state to save."}
      </p>

      {captureBusy === true ? (
        <p
          className="instrument__field-hint"
          data-testid="source-save-busy"
          aria-busy="true"
        >
          Saving the running sound…
        </p>
      ) : null}
    </div>
  );
}
