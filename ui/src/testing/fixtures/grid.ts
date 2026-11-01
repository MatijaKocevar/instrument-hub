import {
  createGridModel,
  type GridFamily,
  type MeterSnapshot,
} from "../../features/grid";

export const GRID_FAMILIES: GridFamily[] = [
  {
    id: "keys",
    name: "Keys",
    patches: [
      {
        id: "piano",
        name: "Grand Piano",
        slot: 0,
        colorIndex: 0,
        warmState: "warm",
        latencySamples: 0,
        latencyMs: 0,
      },
      {
        id: "strings",
        name: "Warm Strings",
        slot: 1,
        colorIndex: 0,
        warmState: "warm",
        latencySamples: 72,
        latencyMs: 1.5,
      },
    ],
  },
  {
    id: "drums",
    name: "Drums",
    patches: [
      {
        id: "kit",
        name: "Acoustic Kit",
        slot: 2,
        colorIndex: 1,
        warmState: "active",
        latencySamples: 0,
        latencyMs: 0,
      },
    ],
  },
  {
    id: "bass",
    name: "Bass",
    patches: [
      {
        id: "sub",
        name: "Sub Bass",
        slot: 3,
        colorIndex: 2,
        warmState: "warm",
        latencySamples: 240,
        latencyMs: 5,
      },
    ],
  },
  {
    id: "pads",
    name: "Pads",
    patches: [
      {
        id: "air",
        name: "Air Pad",
        slot: 4,
        colorIndex: 3,
        warmState: "warm",
        latencySamples: 120,
        latencyMs: 2.5,
      },
    ],
  },
  {
    id: "lead",
    name: "Lead",
    patches: [
      {
        id: "saw",
        name: "Saw Lead",
        slot: 5,
        colorIndex: 4,
        warmState: "warm",
        latencySamples: 48,
        latencyMs: 1,
      },
    ],
  },
  {
    id: "fx",
    name: "FX",
    patches: [
      {
        id: "riser",
        name: "Riser",
        slot: 6,
        colorIndex: 5,
        warmState: "warm",
        latencySamples: 96,
        latencyMs: 2,
      },
    ],
  },
  {
    id: "vocal",
    name: "Vocal",
    patches: [
      {
        id: "chop",
        name: "Vocal Chop",
        slot: 7,
        colorIndex: 6,
        warmState: "warm",
        latencySamples: 24,
        latencyMs: 0.5,
      },
    ],
  },
  {
    id: "perc",
    name: "Percussion",
    patches: [
      {
        id: "shaker",
        name: "Shaker",
        slot: null,
        colorIndex: 7,
        warmState: "cold",
      },
    ],
  },
];

export const GRID_MODEL = createGridModel(GRID_FAMILIES, 2);

export const GRID_METERS: MeterSnapshot = {
  connected: true,
  sequence: 42,
  timestampMs: 1400.5,
  masterPeak: 0.31,
  slots: {
    0: { slot: 0, peak: 0.12, gain: 1, armed: false },
    1: { slot: 1, peak: 0.24, gain: 1, armed: false },
    2: { slot: 2, peak: 0.5, gain: 1, armed: true },
    3: { slot: 3, peak: 0.05, gain: 0.8, armed: false },
    4: { slot: 4, peak: 0, gain: 0.4, armed: false },
    5: { slot: 5, peak: 0.08, gain: 0.2, armed: false },
    6: { slot: 6, peak: 0, gain: 0, armed: false },
    7: { slot: 7, peak: 0, gain: 0, armed: false },
  },
};
