import type { ControlAudioChannel, ControlAudioState } from "../../api/client";

const FINE_MS_THRESHOLD = 10;

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
