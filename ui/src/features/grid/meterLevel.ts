export const METER_FLOOR_DB = -60;
export const METER_CEIL_DB = 0;

export interface MeterLevel {
  readonly db: number;
  readonly percent: number;
  readonly clipped: boolean;
}

export function meterLevelForPeak(peak: number): MeterLevel {
  if (!Number.isFinite(peak) || peak <= 0)
    return { db: METER_FLOOR_DB, percent: 0, clipped: false };

  const db = 20 * Math.log10(peak);
  const clamped = Math.min(METER_CEIL_DB, Math.max(METER_FLOOR_DB, db));

  return {
    db,
    percent:
      ((clamped - METER_FLOOR_DB) / (METER_CEIL_DB - METER_FLOOR_DB)) * 100,
    clipped: peak >= 1,
  };
}
