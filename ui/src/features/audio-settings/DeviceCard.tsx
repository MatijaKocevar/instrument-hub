import type { ReactElement } from "react";

import type {
  ControlAudioDevice,
  ControlAudioPatch,
  ControlAudioState,
} from "../../api/client";
import { deviceDescription, isCurrentDevice, unavailableReason } from "./model";

interface DeviceCardProps {
  readonly audio: ControlAudioState;
  readonly device: ControlAudioDevice;
  readonly disabled: boolean;
  readonly onChange?: ((patch: ControlAudioPatch) => void) | undefined;
}

export function DeviceCard({
  audio,
  device,
  disabled,
  onChange,
}: DeviceCardProps): ReactElement {
  const available = device.available !== false;
  const selected = isCurrentDevice(audio, device);

  return (
    <button
      type="button"
      role="radio"
      aria-checked={selected}
      className="audio-settings__device-card"
      data-testid="device-card"
      data-device={device.name}
      data-available={String(available)}
      data-selected={String(selected)}
      disabled={disabled || !available}
      onClick={() => {
        onChange?.({ deviceType: device.type, deviceName: device.name });
      }}
    >
      <span className="audio-settings__device-name" data-testid="device-name">
        {device.name}
      </span>
      <span
        className="audio-settings__device-description"
        data-testid="device-description"
      >
        {deviceDescription(device)}
      </span>
      {available ? (
        selected ? (
          <span className="audio-settings__device-badge">Selected</span>
        ) : null
      ) : (
        <span
          className="audio-settings__device-reason"
          data-testid="device-reason"
        >
          {unavailableReason(device)}
        </span>
      )}
    </button>
  );
}
