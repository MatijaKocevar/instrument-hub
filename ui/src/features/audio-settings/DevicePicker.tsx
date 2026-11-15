import { useState, type ReactElement } from "react";

import type { ControlAudioPatch, ControlAudioState } from "../../api/client";
import { DeviceCard } from "./DeviceCard";
import { isCurrentDevice, pickerDevices, rawDevices } from "./model";

export interface DevicePickerProps {
  readonly audio: ControlAudioState;
  readonly busy: boolean;
  readonly onChange?: ((patch: ControlAudioPatch) => void) | undefined;
  readonly initialAdvanced?: boolean;
}

export function DevicePicker({
  audio,
  busy,
  onChange,
  initialAdvanced = false,
}: DevicePickerProps): ReactElement {
  const [advanced, setAdvanced] = useState(initialAdvanced);
  const devices = [...pickerDevices(audio)].sort((a, b) => {
    const selectedA = isCurrentDevice(audio, a) ? 0 : 1;
    const selectedB = isCurrentDevice(audio, b) ? 0 : 1;

    return selectedA - selectedB;
  });
  const raw = rawDevices(audio);
  const disabled = busy || onChange === undefined;
  const rawValue = raw.some(
    (device) =>
      device.type === audio.deviceType && device.name === audio.deviceName,
  )
    ? audio.deviceName
    : "";

  return (
    <section
      className="audio-settings__picker"
      aria-label="Output device"
      data-testid="device-picker"
    >
      <header className="audio-settings__picker-head">
        <div>
          <p className="audio-settings__eyebrow">Output</p>
          <h3 className="audio-settings__picker-title">Device</h3>
        </div>
        <button
          type="button"
          className="audio-settings__advanced-toggle"
          data-testid="device-advanced-toggle"
          aria-expanded={advanced}
          aria-controls="device-advanced-list"
          onClick={() => {
            setAdvanced((open) => !open);
          }}
        >
          Advanced
        </button>
      </header>

      <div
        className="audio-settings__device-grid"
        role="radiogroup"
        aria-label="Audio devices"
        data-testid="device-list"
      >
        {devices.length === 0 ? (
          <span className="audio-settings__muted">No audio devices found</span>
        ) : (
          devices.map((device) => (
            <DeviceCard
              key={`${device.type}-${device.name}`}
              audio={audio}
              device={device}
              disabled={disabled}
              onChange={onChange}
            />
          ))
        )}
      </div>

      <div
        id="device-advanced-list"
        className="audio-settings__advanced"
        data-testid="device-advanced"
        data-open={String(advanced)}
        hidden={!advanced}
      >
        <label className="audio-settings__control">
          <span>Raw ALSA PCM</span>
          <select
            data-testid="device-select"
            value={rawValue}
            disabled={disabled || raw.length === 0}
            onChange={(event) => {
              const device = raw.find(
                (entry) => entry.name === event.target.value,
              );

              if (device !== undefined)
                onChange?.({
                  deviceType: device.type,
                  deviceName: device.name,
                });
            }}
          >
            <option value="" disabled>
              {raw.length === 0 ? "No raw PCM devices" : "Choose a raw PCM…"}
            </option>
            {raw.map((device) => (
              <option value={device.name} key={`${device.type}-${device.name}`}>
                {device.description === undefined ||
                device.description.length === 0
                  ? device.name
                  : `${device.name} — ${device.description}`}
              </option>
            ))}
          </select>
        </label>
        <p className="audio-settings__advanced-note">
          Raw ALSA PCM names bypass the card picker. Use them only when the
          device you need is not listed above.
        </p>
      </div>
    </section>
  );
}
