import type { ReactElement } from "react";

import type { ControlAudioMeasurement } from "../../api/client";
import { formatMs } from "./model";

function Measurement({
  measurement,
  error,
}: {
  measurement: ControlAudioMeasurement | null | undefined;
  error: string | null | undefined;
}): ReactElement {
  if (error !== null && error !== undefined && error.length > 0) {
    return (
      <span
        className="audio-settings__measure-error"
        data-testid="measurement-error"
      >
        {error}
      </span>
    );
  }

  if (measurement === null || measurement === undefined) {
    return (
      <span
        className="audio-settings__muted"
        data-testid="measure-result"
        data-valid="none"
      >
        Not measured yet
      </span>
    );
  }

  if (!measurement.valid) {
    return (
      <span
        className="audio-settings__measure-error"
        data-testid="measure-result"
        data-valid="false"
      >
        {measurement.error.length > 0
          ? measurement.error
          : "Measurement failed"}
      </span>
    );
  }

  return (
    <span
      className="audio-settings__measure-result"
      data-testid="measure-result"
      data-valid="true"
    >
      <strong>{formatMs(measurement.roundTripMs)} ms</strong>
      <span>
        {measurement.roundTripSamples} samples · expected{" "}
        {formatMs(measurement.expectedMs)} ms · confidence{" "}
        {Math.round(measurement.confidence * 100)}%
      </span>
    </span>
  );
}

interface MeasureControlProps {
  readonly measurement: ControlAudioMeasurement | null | undefined;
  readonly error: string | null | undefined;
  readonly busy: boolean;
  readonly onMeasure?: (() => void) | undefined;
}

export function MeasureControl({
  measurement,
  error,
  busy,
  onMeasure,
}: MeasureControlProps): ReactElement {
  return (
    <div className="audio-settings__measure">
      <button
        type="button"
        data-testid="measure-button"
        disabled={busy || onMeasure === undefined}
        onClick={() => {
          onMeasure?.();
        }}
      >
        {busy ? "Working…" : "Measure"}
      </button>
      <Measurement measurement={measurement} error={error} />
    </div>
  );
}
