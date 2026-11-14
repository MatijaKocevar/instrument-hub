import type { ReactElement } from "react";

import type { ControlFxTunerState } from "../../api/client";
import "./tuner.css";

interface TunerGaugeProps {
  readonly reading: ControlFxTunerState | null;
  readonly active: boolean;
}

const noteNames = [
  "C",
  "C#",
  "D",
  "D#",
  "E",
  "F",
  "F#",
  "G",
  "G#",
  "A",
  "A#",
  "B",
] as const;

const centerX = 110;
const centerY = 118;
const radius = 82;
const maximumAngle = 55;
const maximumCents = 50;

function angleForCents(cents: number): number {
  const clamped = Math.max(-maximumCents, Math.min(maximumCents, cents));

  return (clamped / maximumCents) * maximumAngle;
}

function pointAt(angleDegrees: number, distance: number): [number, number] {
  const radians = ((angleDegrees - 90) * Math.PI) / 180;

  return [
    centerX + distance * Math.cos(radians),
    centerY + distance * Math.sin(radians),
  ];
}

function arcPath(fromCents: number, toCents: number, distance: number): string {
  const [startX, startY] = pointAt(angleForCents(fromCents), distance);
  const [endX, endY] = pointAt(angleForCents(toCents), distance);

  return `M ${startX.toFixed(2)} ${startY.toFixed(2)} A ${String(distance)} ${String(distance)} 0 0 1 ${endX.toFixed(2)} ${endY.toFixed(2)}`;
}

function noteLabel(midiNote: number): string {
  const noteIndex = ((midiNote % 12) + 12) % 12;
  const octave = Math.floor(midiNote / 12) - 1;

  return `${noteNames[noteIndex] ?? "?"}${String(octave)}`;
}

export function TunerGauge({ reading, active }: TunerGaugeProps): ReactElement {
  const valid = active && reading?.valid === true;
  const cents = valid ? (reading?.cents ?? 0) : 0;
  const roundedCents = Math.round(cents);
  const inTune = valid && Math.abs(cents) <= 3;
  const hasSignal = active && (reading?.levelDb ?? -120) > -55 && !valid;
  const status = !active
    ? "Not armed"
    : valid
      ? inTune
        ? "In tune"
        : cents < 0
          ? "Flat"
          : "Sharp"
      : hasSignal
        ? "Listening…"
        : "No signal";
  const note = valid && reading !== null ? noteLabel(reading.midiNote) : "—";
  const centsLabel = valid
    ? `${roundedCents > 0 ? "+" : ""}${String(roundedCents)}¢`
    : "—";
  const frequencyLabel = valid
    ? `${(reading?.frequencyHz ?? 0).toFixed(2)} Hz`
    : "A4 440 Hz";
  const angle = valid ? angleForCents(cents) : 0;
  const stateClass = !active
    ? "tuner--idle"
    : valid
      ? inTune
        ? "tuner--in-tune"
        : "tuner--off"
      : "tuner--idle";

  const ticks = [];

  for (let tick = -maximumCents; tick <= maximumCents; tick += 5) {
    const major = tick % 25 === 0;
    const angleDegrees = angleForCents(tick);
    const [x1, y1] = pointAt(angleDegrees, radius - (major ? 10 : 5));
    const [x2, y2] = pointAt(angleDegrees, radius);

    ticks.push(
      <line
        key={`tick-${String(tick)}`}
        className={`tuner__tick${major ? " tuner__tick--major" : ""}`}
        x1={x1}
        y1={y1}
        x2={x2}
        y2={y2}
      />,
    );
  }

  const labels = [-50, -25, 0, 25, 50].map((tick) => {
    const [x, y] = pointAt(angleForCents(tick), radius + 13);

    return (
      <text
        key={`label-${String(tick)}`}
        className="tuner__tick-label"
        x={x}
        y={y}
      >
        {tick > 0 ? `+${String(tick)}` : String(tick)}
      </text>
    );
  });

  return (
    <div
      className={`tuner ${stateClass}`}
      data-testid="fx-tuner-gauge"
      data-valid={valid ? "true" : "false"}
      data-status={status}
      data-note={note}
      data-cents={valid ? String(roundedCents) : ""}
    >
      <div className="tuner__readout">
        <span className="tuner__note" data-testid="fx-tuner-note">
          {note}
        </span>
        <span className="tuner__cents" data-testid="fx-tuner-cents">
          {centsLabel}
        </span>
      </div>

      <div className="tuner__gauge-wrap">
        <svg
          className="tuner__gauge"
          viewBox="0 0 220 130"
          role="img"
          aria-label={`Tuner: ${note}, ${status}${valid ? `, ${centsLabel}` : ""}`}
        >
          <path
            className="tuner__arc"
            d={arcPath(-maximumCents, maximumCents, radius)}
          />
          <path
            className="tuner__arc tuner__arc--near"
            d={arcPath(-15, -3, radius)}
          />
          <path
            className="tuner__arc tuner__arc--near"
            d={arcPath(3, 15, radius)}
          />
          <path
            className="tuner__arc tuner__arc--in-tune"
            d={arcPath(-3, 3, radius)}
          />
          {ticks}
          {labels}
          <g
            className="tuner__needle"
            style={{
              transform: `rotate(${angle.toFixed(2)}deg)`,
              transformOrigin: `${String(centerX)}px ${String(centerY)}px`,
            }}
          >
            <line
              className="tuner__needle-line"
              x1={centerX}
              y1={centerY}
              x2={centerX}
              y2={centerY - (radius - 26)}
            />
          </g>
          <circle className="tuner__pivot" cx={centerX} cy={centerY} r={5} />
          <circle
            className="tuner__pivot-core"
            cx={centerX}
            cy={centerY}
            r={2}
          />
        </svg>
      </div>

      <div className="tuner__meta">
        <span className="tuner__status" data-testid="fx-tuner-status">
          {status}
        </span>
        <span className="tuner__frequency" data-testid="fx-tuner-frequency">
          {frequencyLabel}
        </span>
      </div>
    </div>
  );
}
