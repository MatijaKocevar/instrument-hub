export interface TrackVolumeStep {
  readonly step: number;
  readonly label: string;
  readonly percent: string;
  readonly gain: number;
  readonly db: number;
}

export interface TrackVolumeTick {
  readonly label: string;
  readonly db: number;
  readonly position: number;
}

const BOOST_GAIN = 1.4125375446227544;

export const TRACK_MIN_DB = -60;
export const TRACK_MAX_DB = 3;
export const TRACK_MIN_GAIN = 0;
export const TRACK_MAX_GAIN = BOOST_GAIN;
export const TRACK_DEFAULT_GAIN = 1;

function dbForGain(gain: number): number {
  return gain <= 0 ? TRACK_MIN_DB : 20 * Math.log10(gain);
}

function positionForDb(db: number): number {
  return Math.min(
    1,
    Math.max(0, (db - TRACK_MIN_DB) / (TRACK_MAX_DB - TRACK_MIN_DB)),
  );
}

function tickLabel(db: number): string {
  if (db <= TRACK_MIN_DB) return "−∞";
  if (db > 0) return `+${String(db)} dB`;
  if (db === 0) return "0 dB";

  return `−${String(Math.abs(db))} dB`;
}

const STEP_GAINS = [0, 0.5, 0.75, 1, BOOST_GAIN] as const;
const STEP_LABELS = ["0%", "50%", "75%", "100%", "+3 dB"] as const;
const TICK_DBS = [3, 0, -6, -12, -24, -36, -48, TRACK_MIN_DB] as const;

export const TRACK_VOLUME_STEPS: readonly TrackVolumeStep[] = STEP_GAINS.map(
  (gain, step) => ({
    step,
    label: STEP_LABELS[step] ?? STEP_LABELS[0],
    percent: `${String(Math.round(gain * 100))}%`,
    gain,
    db: dbForGain(gain),
  }),
);

export const TRACK_VOLUME_TICKS: readonly TrackVolumeTick[] = TICK_DBS.map(
  (db) => ({
    label: tickLabel(db),
    db,
    position: positionForDb(db),
  }),
);

const STAGE_COUNT = TRACK_VOLUME_STEPS.length;

export const TRACK_MAX_STEP = STAGE_COUNT - 1;

export function clampTrackVolumeGain(gain: number): number {
  if (!Number.isFinite(gain)) return TRACK_DEFAULT_GAIN;

  return Math.min(TRACK_MAX_GAIN, Math.max(TRACK_MIN_GAIN, gain));
}

export function trackDbForGain(gain: number): number {
  return dbForGain(clampTrackVolumeGain(gain));
}

export function trackGainForDb(db: number): number {
  const clamped = Math.min(TRACK_MAX_DB, Math.max(TRACK_MIN_DB, db));

  return clamped <= TRACK_MIN_DB
    ? TRACK_MIN_GAIN
    : clampTrackVolumeGain(10 ** (clamped / 20));
}

export function trackGainForStep(step: number): number {
  return TRACK_VOLUME_STEPS[Math.round(step)]?.gain ?? TRACK_DEFAULT_GAIN;
}

export function trackVolumeStepForGain(gain: number): number {
  const value = clampTrackVolumeGain(gain);

  for (let step = TRACK_MAX_STEP; step > 0; step -= 1) {
    if (value + 1e-6 >= (TRACK_VOLUME_STEPS[step]?.gain ?? 0)) return step;
  }

  return 0;
}

export function trackGainForPosition(position: number): number {
  const clamped = Math.min(1, Math.max(0, position));

  if (clamped <= 0) return TRACK_MIN_GAIN;

  return trackGainForDb(TRACK_MIN_DB + clamped * (TRACK_MAX_DB - TRACK_MIN_DB));
}

export function trackVolumePositionForGain(gain: number): number {
  const value = clampTrackVolumeGain(gain);

  return value <= 0 ? 0 : positionForDb(dbForGain(value));
}

export function trackGainForPointerRatio(ratio: number): number {
  return trackGainForPosition(ratio);
}

export function trackVolumePercentForPosition(position: number): number {
  return Math.min(1, Math.max(0, position)) * 100;
}
