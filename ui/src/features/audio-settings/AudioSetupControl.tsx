import type { ReactElement } from "react";

import type { ControlAudioWizard } from "../../api/client";
import { Spinner } from "../loading";
import { AudioSetupDialog } from "./AudioSetupDialog";

interface AudioSetupControlProps {
  readonly wizard: ControlAudioWizard | null;
  readonly busy: boolean;
  readonly onRecommend?: (() => void) | undefined;
  readonly onApply?: (() => void) | undefined;
  readonly onDismiss?: (() => void) | undefined;
}

export function AudioSetupControl({
  wizard,
  busy,
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
        {busy && wizard === null ? (
          <Spinner label="Testing the audio setup" />
        ) : null}
        {busy && wizard === null ? "Testing…" : "Test & recommend"}
      </button>
      <span className="audio-settings__muted">
        Probes 64, 96 and 128 frames at every supported rate.
      </span>

      {busy || wizard !== null ? (
        <AudioSetupDialog
          wizard={wizard}
          busy={busy}
          onApply={onApply}
          onDismiss={onDismiss}
        />
      ) : null}
    </div>
  );
}
