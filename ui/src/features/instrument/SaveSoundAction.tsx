import type { ReactElement } from "react";

import type { ControlPatch, ControlPatchStateCapture } from "../../api/client";
import { formatStateSize } from "./format";
import { isPluginSource } from "./source";

interface SaveSoundActionProps
{
    readonly patch: ControlPatch;
    /** True while an explicit save of the patch's sound is in flight. */
    readonly captureBusy?: boolean;
    /** Confirmation of the most recent explicit save for this patch. */
    readonly capture?: ControlPatchStateCapture | null;
    /** Message from the most recent failed explicit save. */
    readonly captureError?: string | null;
    readonly onCaptureState?: (slot: number) => void;
}

/** The explicit save-sound action with its confirmation, busy and error states. */
export function SaveSoundAction ({
    patch,
    captureBusy,
    capture,
    captureError,
    onCaptureState,
}: SaveSoundActionProps): ReactElement
{
    const isPlugin = isPluginSource (patch);

    return (
        <div className="instrument__save"
             data-testid="patch-state-save"
             data-slot={patch.slot}
             data-enabled={isPlugin ? "true" : "false"}
             data-busy={captureBusy === true ? "true" : "false"}>
            <button
                className="instrument__button instrument__button--save"
                type="button"
                data-testid="source-save-state"
                data-slot={patch.slot}
                disabled={! isPlugin || captureBusy === true || onCaptureState === undefined}
                onClick={() => { onCaptureState?.(patch.slot); }}
            >
                Save sound to patch
            </button>

            <p className="instrument__field-hint" data-testid="source-save-hint">
                {isPlugin
                    ? "Captures the running plug-in's current sound into this patch without switching away."
                    : "Only plug-in sources have a sound state to save."}
            </p>

            {captureBusy === true
                ? <p className="instrument__field-hint"
                      data-testid="source-save-busy"
                      aria-busy="true">
                      Saving the running sound…
                  </p>
                : null}

            {captureError !== null && captureError !== undefined && captureError.length > 0
                ? <p className="instrument__error"
                      role="alert"
                      data-testid="source-save-error">
                      {captureError}
                  </p>
                : null}

            {capture !== null && capture !== undefined && capture.slot === patch.slot
                ? <p className="instrument__field-hint instrument__save-status"
                      role="status"
                      data-testid="source-save-status"
                      data-size-bytes={capture.sizeBytes}
                      data-slot={capture.slot}>
                      Saved to patch · {formatStateSize (capture.sizeBytes)}
                  </p>
                : null}
        </div>
    );
}
