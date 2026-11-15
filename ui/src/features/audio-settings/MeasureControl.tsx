import { useState } from "react";
import type { ReactElement } from "react";

import type { ControlAudioMeasurement } from "../../api/client";
import type { MeasurementSource } from "../../api/audio";
import { Spinner } from "../loading";
import { formatMs } from "./model";

function Measurement({
  measurement,
}: {
  measurement: ControlAudioMeasurement | null | undefined;
}): ReactElement {
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
        Measurement failed
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
  readonly busy: boolean;
  readonly onMeasure?: ((source: MeasurementSource) => void) | undefined;
}

export function MeasureControl({
  measurement,
  busy,
  onMeasure,
}: MeasureControlProps): ReactElement {
  const [source, setSource] = useState<MeasurementSource>("device");

  return (
    <div className="audio-settings__measure">
      <select
        className="audio-settings__measure-source"
        data-testid="measure-source"
        aria-label="Latency measurement loopback"
        value={source}
        disabled={busy}
        onChange={(event) => {
          setSource(event.target.value as MeasurementSource);
        }}
      >
        <option value="device">Selected device (cable)</option>
        <option value="loopback">ALSA loopback (internal)</option>
      </select>
      <button
        type="button"
        data-testid="measure-button"
        disabled={busy || onMeasure === undefined}
        onClick={() => {
          onMeasure?.(source);
        }}
      >
        {busy ? <Spinner label="Measuring latency" /> : null}
        {busy ? "Working…" : "Measure"}
      </button>
      <Measurement measurement={measurement} />
    </div>
  );
}
