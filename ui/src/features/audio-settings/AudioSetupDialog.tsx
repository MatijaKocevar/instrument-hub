import type { ReactElement } from "react";

import type { ControlAudioWizard } from "../../api/client";
import { formatMs, formatSampleRate } from "./model";

interface AudioSetupDialogProps {
  readonly wizard: ControlAudioWizard | null;
  readonly busy: boolean;
  readonly error: string | null;
  readonly onApply?: (() => void) | undefined;
  readonly onDismiss?: (() => void) | undefined;
}

export function AudioSetupDialog({
  wizard,
  busy,
  error,
  onApply,
  onDismiss,
}: AudioSetupDialogProps): ReactElement {
  return (
    <div
      className="audio-wizard"
      role="dialog"
      aria-modal="true"
      aria-label="Audio setup recommendation"
      data-testid="audio-wizard"
      data-valid={wizard?.valid ?? false}
    >
      <div className="audio-wizard__header">
        <h2>Test &amp; recommend</h2>
        <button
          type="button"
          className="audio-wizard__close"
          data-testid="audio-wizard-dismiss"
          aria-label="Close"
          disabled={busy}
          onClick={() => {
            onDismiss?.();
          }}
        >
          ×
        </button>
      </div>

      {busy && wizard === null ? (
        <p data-testid="audio-wizard-busy">
          Testing the soundcard at 64, 96 and 128 frames…
        </p>
      ) : null}

      {error !== null && error.length > 0 ? (
        <p className="audio-settings__error" data-testid="audio-wizard-error">
          {error}
        </p>
      ) : null}

      {wizard !== null && wizard.valid ? (
        <>
          <p
            className="audio-wizard__summary"
            data-testid="audio-wizard-summary"
          >
            <strong>{wizard.deviceName}</strong> ·{" "}
            {wizard.profile === "pro-audio" ? "pro-audio" : "standard"} profile
            · {formatSampleRate(wizard.sampleRate)} / {wizard.bufferSize} frames
            · {formatMs(wizard.latencyMs)} ms
          </p>

          <table className="audio-wizard__table">
            <thead>
              <tr>
                <th>Rate</th>
                <th>Buffer</th>
                <th>Latency</th>
                <th>Xruns</th>
                <th>Result</th>
              </tr>
            </thead>
            <tbody>
              {wizard.candidates.map((candidate) => (
                <tr
                  key={`${candidate.sampleRate}-${candidate.bufferSize}`}
                  data-testid="audio-wizard-candidate"
                  data-stable={candidate.stable}
                  data-recommended={candidate.recommended}
                >
                  <td>{formatSampleRate(candidate.sampleRate)}</td>
                  <td>{candidate.bufferSize}</td>
                  <td>{formatMs(candidate.latencyMs)} ms</td>
                  <td>{candidate.xruns}</td>
                  <td>
                    {candidate.recommended
                      ? "Recommended"
                      : candidate.stable
                        ? "Stable"
                        : candidate.opened
                          ? "Xruns"
                          : "Unavailable"}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>

          <p
            className="audio-wizard__tradeoff"
            data-testid="audio-wizard-tradeoff"
          >
            {wizard.tradeoff}
          </p>

          {wizard.measuredMs > 0 ? (
            <p className="audio-settings__muted">
              Measured loopback round trip: {formatMs(wizard.measuredMs)} ms
            </p>
          ) : null}

          <div className="audio-wizard__actions">
            <button
              type="button"
              className="audio-wizard__apply"
              data-testid="audio-wizard-apply"
              disabled={busy || onApply === undefined}
              onClick={() => {
                onApply?.();
              }}
            >
              {busy ? "Applying…" : "Apply"}
            </button>
            <button
              type="button"
              data-testid="audio-wizard-cancel"
              disabled={busy}
              onClick={() => {
                onDismiss?.();
              }}
            >
              Cancel
            </button>
          </div>
        </>
      ) : null}
    </div>
  );
}
