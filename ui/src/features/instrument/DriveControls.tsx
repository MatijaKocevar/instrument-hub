import type { ReactElement } from "react";

import type { ControlFxInsert, ControlFxInsertUpdate } from "../../api/client";
import {
  DRIVE_PRESETS,
  driveParametersForPreset,
  drivePresetForModel,
} from "./drive";

interface DriveControlsProps {
  readonly insert: ControlFxInsert;
  readonly onUpdate?: (update: ControlFxInsertUpdate) => void;
}

interface DriveSliderProps {
  readonly id: string;
  readonly label: string;
  readonly min: number;
  readonly max: number;
  readonly step: number;
  readonly value: number;
  readonly display: string;
  readonly testId: string;
  readonly onChange: (value: number) => void;
}

function clampNumber(
  value: number | undefined,
  min: number,
  max: number,
  fallback: number,
): number {
  if (value === undefined || !Number.isFinite(value)) return fallback;

  return Math.min(max, Math.max(min, value));
}

function DriveSlider({
  id,
  label,
  min,
  max,
  step,
  value,
  display,
  testId,
  onChange,
}: DriveSliderProps): ReactElement {
  return (
    <label className="instrument__drive-row" htmlFor={id}>
      <span className="instrument__drive-label">{label}</span>
      <input
        id={id}
        className="instrument__drive-fader"
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        data-testid={testId}
        data-value={value.toFixed(3)}
        onChange={(event) => {
          onChange(Number(event.currentTarget.value));
        }}
      />
      <output className="instrument__drive-value">{display}</output>
    </label>
  );
}

export function DriveControls({
  insert,
  onUpdate,
}: DriveControlsProps): ReactElement {
  const preset = drivePresetForModel(insert.parameters.model ?? 0);
  const drive = clampNumber(insert.parameters.drive, 0, 1, preset.drive);
  const tone = clampNumber(insert.parameters.tone, 0, 1, preset.tone);
  const levelDb = clampNumber(
    insert.parameters.levelDb,
    -24,
    12,
    preset.levelDb,
  );
  const cab = (insert.parameters.cab ?? preset.cab) >= 0.5;
  const baseId = `drive-${String(insert.index)}`;

  const updateParameters = (parameters: Record<string, number>): void => {
    onUpdate?.({ parameters });
  };

  return (
    <details className="instrument__drive" data-testid="fx-drive">
      <summary className="instrument__drive-summary">
        <span>Drive controls</span>
        <span className="instrument__drive-summary-model">{preset.label}</span>
      </summary>

      <div className="instrument__drive-body">
        <label
          className="instrument__drive-row instrument__drive-row--model"
          htmlFor={`${baseId}-preset`}
        >
          <span className="instrument__drive-label">Preset</span>
          <select
            id={`${baseId}-preset`}
            className="instrument__drive-select"
            value={String(preset.model)}
            data-testid="fx-drive-preset"
            onChange={(event) => {
              const next = drivePresetForModel(
                Number(event.currentTarget.value),
              );

              onUpdate?.({
                name: next.label,
                parameters: driveParametersForPreset(next),
              });
            }}
          >
            {DRIVE_PRESETS.map((candidate) => (
              <option key={candidate.id} value={String(candidate.model)}>
                {candidate.label}
              </option>
            ))}
          </select>
        </label>

        <DriveSlider
          id={`${baseId}-drive`}
          label="Drive"
          min={0}
          max={1}
          step={0.01}
          value={drive}
          display={`${String(Math.round(drive * 100))}%`}
          testId="fx-drive-amount"
          onChange={(value) => {
            updateParameters({ drive: value });
          }}
        />

        <DriveSlider
          id={`${baseId}-tone`}
          label="Tone"
          min={0}
          max={1}
          step={0.01}
          value={tone}
          display={`${String(Math.round(tone * 100))}%`}
          testId="fx-drive-tone"
          onChange={(value) => {
            updateParameters({ tone: value });
          }}
        />

        <DriveSlider
          id={`${baseId}-level`}
          label="Level"
          min={-24}
          max={12}
          step={0.5}
          value={levelDb}
          display={`${levelDb.toFixed(1)} dB`}
          testId="fx-drive-level"
          onChange={(value) => {
            updateParameters({ levelDb: value });
          }}
        />

        <button
          className={`instrument__toggle instrument__toggle--small instrument__drive-cab${cab ? " instrument__toggle--on" : ""}`}
          type="button"
          aria-pressed={cab}
          data-testid="fx-drive-cab"
          data-cab={cab ? "true" : "false"}
          onClick={() => {
            updateParameters({ cab: cab ? 0 : 1 });
          }}
        >
          Cab {cab ? "On" : "Off"}
        </button>
      </div>
    </details>
  );
}
