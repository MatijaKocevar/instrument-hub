import type { ReactElement } from "react";

import type { ControlAudioWizard } from "../../api/client";
import { AudioSetupDialog } from "./AudioSetupDialog";

interface AudioSetupControlProps {
  readonly wizard: ControlAudioWizard | null;
  readonly busy: boolean;
  readonly error: string | null;
  readonly onRecommend?: (() => void) | undefined;
  readonly onApply?: (() => void) | undefined;
  readonly onDismiss?: (() => void) | undefined;
}

export function AudioSetupControl({
  wizard,
  busy,
  error,
  onRecommend,
  onApply,
  onDismiss,
}: AudioSetupControlProps): ReactElement {
  return (
    <div className="audio-settings__wizard-control">
      <button
        type="button"
        className="audio-settings__wizard-button"
        data-testid="audio-wizard-button"
        disabled={busy || onRecommend === undefined}
        onClick={() => {
          onRecommend?.();
        }}
      >
        {busy && wizard === null ? "Testing…" : "Test & recommend"}
      </button>
      <span className="audio-settings__muted">
        Probes 64, 96 and 128 frames at every supported rate.
      </span>

      {busy || wizard !== null || error !== null ? (
        <AudioSetupDialog
          wizard={wizard}
          busy={busy}
          error={error}
          onApply={onApply}
          onDismiss={onDismiss}
        />
      ) : null}
    </div>
  );
}
