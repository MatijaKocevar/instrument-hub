export interface MeterSlotReading {
  readonly slot: number;
  readonly peak: number;
  readonly gain: number;
  readonly armed: boolean;
}

export interface MeterSnapshot {
  readonly connected: boolean;
  readonly sequence: number;
  readonly timestampMs: number;
  readonly masterPeak: number;
  readonly slots: Readonly<Record<number, MeterSlotReading>>;
}

export const EMPTY_METERS: MeterSnapshot = {
  connected: false,
  sequence: 0,
  timestampMs: 0,
  masterPeak: 0,
  slots: {},
};

function finiteOrZero(value: unknown): number {
  return typeof value === "number" && Number.isFinite(value) ? value : 0;
}

export interface ParsedMeterFrame {
  sequence: number;
  timestampMs: number;
  masterPeak: number;
  slots: MeterSlotReading[];
}

export function parseMeterFrame(value: unknown): ParsedMeterFrame | null {
  let candidate = value;

  if (typeof candidate === "string") {
    try {
      candidate = JSON.parse(candidate);
    } catch {
      return null;
    }
  }

  if (candidate === null || typeof candidate !== "object") return null;

  const frame = candidate as {
    type?: unknown;
    sequence?: unknown;
    timestampMs?: unknown;
    master?: { peak?: unknown };
    slots?: unknown;
  };

  if (frame.type !== "meters" || !Array.isArray(frame.slots)) return null;

  const slots: MeterSlotReading[] = [];

  for (const entry of frame.slots) {
    if (entry === null || typeof entry !== "object") continue;

    const reading = entry as {
      slot?: unknown;
      peak?: unknown;
      gain?: unknown;
      armed?: unknown;
    };

    if (typeof reading.slot !== "number" || !Number.isInteger(reading.slot))
      continue;

    slots.push({
      slot: reading.slot,
      peak: finiteOrZero(reading.peak),
      gain: finiteOrZero(reading.gain),
      armed: reading.armed === true,
    });
  }

  return {
    sequence: finiteOrZero(frame.sequence),
    timestampMs: finiteOrZero(frame.timestampMs),
    masterPeak: finiteOrZero(frame.master?.peak),
    slots,
  };
}
