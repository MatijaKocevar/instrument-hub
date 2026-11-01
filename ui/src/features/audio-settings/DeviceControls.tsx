import type { ChangeEvent, ReactElement } from "react";

import type { ControlAudioPatch, ControlAudioState } from "../../api/client";
import { formatSampleRate } from "./model";

function uniqueSorted(values: number[], current: number): number[] {
  const numbers = values.filter((value) => Number.isFinite(value));

  if (!numbers.includes(current)) numbers.push(current);

  return [...new Set(numbers)].sort((a, b) => a - b);
}

interface DeviceControlsProps {
  readonly audio: ControlAudioState;
  readonly busy: boolean;
  readonly onChange?: ((patch: ControlAudioPatch) => void) | undefined;
}

export function DeviceControls({
  audio,
  busy,
  onChange,
}: DeviceControlsProps): ReactElement {
  const devicesOfType = audio.devices.filter(
    (device) => device.type === audio.deviceType,
  );
  const deviceOptions =
    devicesOfType.length > 0 ? devicesOfType : audio.devices;
  const sampleRates = uniqueSorted(audio.sampleRates, audio.sampleRate);
  const bufferSizes = uniqueSorted(audio.bufferSizes, audio.bufferSize);
  const disabled = busy || onChange === undefined;

  return (
    <div className="audio-settings__controls">
      <label className="audio-settings__control">
        <span>Device type</span>
        <select
          data-testid="device-type-select"
          value={audio.deviceType}
          disabled={disabled}
          onChange={(event: ChangeEvent<HTMLSelectElement>) => {
            onChange?.({ deviceType: event.target.value });
          }}
        >
          {audio.deviceTypes.map((type) => (
            <option value={type} key={type}>
              {type}
            </option>
          ))}
        </select>
      </label>

      <label className="audio-settings__control">
        <span>Device</span>
        <select
          data-testid="device-select"
          value={audio.deviceName}
          disabled={disabled}
          onChange={(event: ChangeEvent<HTMLSelectElement>) => {
            onChange?.({ deviceName: event.target.value });
          }}
        >
          {deviceOptions.map((device) => (
            <option value={device.name} key={`${device.type}-${device.name}`}>
              {device.name}
            </option>
          ))}
        </select>
      </label>

      <label className="audio-settings__control">
        <span>Sample rate</span>
        <select
          data-testid="sample-rate-select"
          value={audio.sampleRate}
          disabled={disabled}
          onChange={(event: ChangeEvent<HTMLSelectElement>) => {
            onChange?.({ sampleRate: Number(event.target.value) });
          }}
        >
          {sampleRates.map((rate) => (
            <option value={rate} key={rate}>
              {formatSampleRate(rate)}
            </option>
          ))}
        </select>
      </label>

      <label className="audio-settings__control">
        <span>Buffer size</span>
        <select
          data-testid="buffer-size-select"
          value={audio.bufferSize}
          disabled={disabled}
          onChange={(event: ChangeEvent<HTMLSelectElement>) => {
            onChange?.({ bufferSize: Number(event.target.value) });
          }}
        >
          {bufferSizes.map((size) => (
            <option value={size} key={size}>
              {size} frames
            </option>
          ))}
        </select>
      </label>
    </div>
  );
}
