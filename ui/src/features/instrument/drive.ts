import type { ControlFxInsertRequest } from "../../api/client";
import { uniqueInsertId } from "./inserts";

export interface DrivePreset {
  readonly id: string;
  readonly label: string;
  readonly model: number;
  readonly drive: number;
  readonly tone: number;
  readonly levelDb: number;
  readonly cab: number;
}

export const DRIVE_PRESETS = [
  {
    id: "overdrive",
    label: "Overdrive",
    model: 0,
    drive: 0.35,
    tone: 0.55,
    levelDb: -8,
    cab: 1,
  },
  {
    id: "blues",
    label: "Blues",
    model: 1,
    drive: 0.45,
    tone: 0.5,
    levelDb: -6,
    cab: 1,
  },
  {
    id: "rock",
    label: "Rock",
    model: 2,
    drive: 0.6,
    tone: 0.6,
    levelDb: -18,
    cab: 1,
  },
  {
    id: "distortion",
    label: "Distortion",
    model: 3,
    drive: 0.75,
    tone: 0.62,
    levelDb: -21,
    cab: 1,
  },
  {
    id: "grunge",
    label: "Grunge",
    model: 4,
    drive: 0.82,
    tone: 0.42,
    levelDb: -21,
    cab: 1,
  },
  {
    id: "fuzz",
    label: "Fuzz",
    model: 5,
    drive: 0.88,
    tone: 0.68,
    levelDb: -24,
    cab: 1,
  },
] as const satisfies readonly DrivePreset[];

export function drivePresetForModel(model: number): DrivePreset {
  const rounded = Math.round(model);

  return (
    DRIVE_PRESETS.find((preset) => preset.model === rounded) ?? DRIVE_PRESETS[0]
  );
}

export function driveModelLabel(model: number): string {
  return drivePresetForModel(model).label;
}

export function driveParametersForPreset(
  preset: DrivePreset,
): Record<string, number> {
  return {
    model: preset.model,
    drive: preset.drive,
    tone: preset.tone,
    levelDb: preset.levelDb,
    cab: preset.cab,
  };
}

export function insertRequestForDrivePreset(
  preset: DrivePreset,
  existingIds: readonly string[],
): ControlFxInsertRequest {
  return {
    type: "drive",
    id: uniqueInsertId(`drive-${preset.id}`, existingIds),
    name: preset.label,
    bypassed: false,
    wet: 1,
    position: -1,
    parameters: driveParametersForPreset(preset),
  };
}
