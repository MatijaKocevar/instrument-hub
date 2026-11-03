import type {
  ControlAudioChannel,
  ControlAudioPatch,
  ControlAudioState,
} from "../../api/client";

const FINE_MS_THRESHOLD = 10;

export interface RestoreFallback {
  readonly device: string;
  readonly reason: string;
}

function quotedAfter(reason: string, prefix: string): string | null {
  const start = `${prefix}"`;

  if (!reason.startsWith(start)) return null;

  const end = reason.indexOf('"', start.length);

  return end < 0 ? null : reason.slice(start.length, end);
}

export function restoreFallback(
  audio: ControlAudioState,
): RestoreFallback | null {
  if (!audio.restore.fallback || audio.restore.usedStoredDevice) return null;

  return {
    device: deviceLabel(audio.deviceType, audio.deviceName),
    reason:
      audio.restore.reason.length > 0
        ? audio.restore.reason
        : "the stored audio device was not available",
  };
}

export function storedDevicePatch(
  audio: ControlAudioState,
): ControlAudioPatch | null {
  if (!audio.restore.fallback) return null;

  const name = quotedAfter(audio.restore.reason, "stored device ");

  if (name !== null) {
    const device = audio.devices.find((entry) => entry.name === name);

    return device === undefined
      ? { deviceName: name }
      : { deviceType: device.type, deviceName: device.name };
  }

  const type = quotedAfter(
    audio.restore.reason,
    "stored no output device of type ",
  );

  if (type === null) return null;

  const device = audio.devices.find((entry) => entry.type === type);

  return device === undefined
    ? { deviceType: type }
    : { deviceType: device.type, deviceName: device.name };
}

export function storedDeviceLabel(patch: ControlAudioPatch): string {
  if (patch.deviceName !== undefined && patch.deviceName.length > 0) {
    return patch.deviceType === undefined || patch.deviceType.length === 0
      ? patch.deviceName
      : deviceLabel(patch.deviceType, patch.deviceName);
  }

  return patch.deviceType ?? "";
}

export function forgetDevicePatch(audio: ControlAudioState): ControlAudioPatch {
  return { deviceType: audio.deviceType, deviceName: audio.deviceName };
}

export function formatMs(milliseconds: number): string {
  if (!Number.isFinite(milliseconds) || milliseconds <= 0) return "0.00";

  const decimals = milliseconds < FINE_MS_THRESHOLD ? 2 : 1;

  return milliseconds.toFixed(decimals);
}

export function formatSampleRate(rate: number): string {
  if (!Number.isFinite(rate) || rate <= 0) return "—";

  const kilohertz = rate / 1000;
  const text = Number.isInteger(kilohertz)
    ? kilohertz.toFixed(0)
    : kilohertz.toFixed(1);

  return `${text} kHz`;
}

export function deviceLabel(type: string, name: string): string {
  return type.length > 0 ? `${name} (${type})` : name;
}

export function activeChannels(channels: ControlAudioChannel[]): number[] {
  return channels
    .filter((channel) => channel.active)
    .map((channel) => channel.index);
}

export function toggleChannel(
  channels: ControlAudioChannel[],
  index: number,
): number[] {
  const active = new Set(activeChannels(channels));

  if (active.has(index)) active.delete(index);
  else active.add(index);

  return [...active].sort((a, b) => a - b);
}

export function channelSummary(channels: ControlAudioChannel[]): string {
  const active = activeChannels(channels).length;

  return `${String(active)} of ${String(channels.length)} active`;
}

export function bufferSizePatch(bufferSize: number): { bufferSize: number } {
  return { bufferSize };
}

export function isOpen(audio: ControlAudioState | null): boolean {
  return audio?.open === true;
}
