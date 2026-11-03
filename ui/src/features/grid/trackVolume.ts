export interface TrackVolumeStep {
  readonly step: number;
  readonly label: string;
  readonly percent: string;
  readonly gain: number;
  readonly db: number;
}

const OVER_GAIN = 1.9952623149688795;

function dbForGain(gain: number): number {
  return gain <= 0 ? -60 : 20 * Math.log10(gain);
}

const STEP_GAINS = [0, 0.25, 0.5, 0.75, 1, OVER_GAIN] as const;
const STEP_LABELS = ["0%", "25%", "50%", "75%", "100%", "+6 dB"] as const;

export const TRACK_VOLUME_STEPS: readonly TrackVolumeStep[] = STEP_GAINS.map(
  (gain, step) => ({
    step,
    label: STEP_LABELS[step] ?? STEP_LABELS[0],
    percent: `${String(Math.round(gain * 100))}%`,
    gain,
    db: dbForGain(gain),
  }),
);

export const TRACK_OVER_STEP = 5;
export const TRACK_MAX_STEP = 5;
export const TRACK_DEFAULT_STEP = 4;

export function clampTrackVolumeStep(step: number): number {
  if (!Number.isInteger(step)) return TRACK_DEFAULT_STEP;

  return Math.min(TRACK_MAX_STEP, Math.max(0, step));
}

export function trackVolumeStep(step: number): TrackVolumeStep {
  return (
    TRACK_VOLUME_STEPS[clampTrackVolumeStep(step)] ?? TRACK_VOLUME_STEPS[0]!
  );
}

export function trackGainForStep(step: number): number {
  return trackVolumeStep(step).gain;
}

export function trackDbForStep(step: number): number {
  return trackVolumeStep(step).db;
}

export function volumeStepForRow(row: number, currentStep: number): number {
  if (row !== 4) return clampTrackVolumeStep(row);

  return clampTrackVolumeStep(currentStep) === 4 ? TRACK_OVER_STEP : 4;
}
