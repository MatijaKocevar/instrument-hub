import type {
  ControlPatchInstrument,
  ControlState,
  ControlTrack,
  ControlTrackHotFx,
} from "../../api/client";

const STEP_GAINS = [0, 0.5, 0.75, 1, 1.4125375446227544] as const;

interface TrackFixtureSpec {
  readonly track: string;
  readonly patch: string;
  readonly instrument: ControlPatchInstrument;
  readonly colorIndex: number;
  readonly volumeStep: number;
  readonly hot: boolean;
}

const SPECS: readonly TrackFixtureSpec[] = [
  {
    track: "Track 1",
    patch: "Studio Grand",
    instrument: { id: "studio-grand", name: "Studio Grand", kind: "init-tone" },
    colorIndex: 0,
    volumeStep: 3,
    hot: true,
  },
  {
    track: "Track 2",
    patch: "Warm Pad",
    instrument: { id: "warm-pad", name: "Warm Pad", kind: "init-tone" },
    colorIndex: 1,
    volumeStep: 3,
    hot: true,
  },
  {
    track: "Track 3",
    patch: "Drawbar Organ",
    instrument: {
      id: "drawbar-organ",
      name: "Drawbar Organ",
      kind: "init-tone",
    },
    colorIndex: 2,
    volumeStep: 3,
    hot: true,
  },
  {
    track: "Track 4",
    patch: "Sub Bass",
    instrument: { id: "sub-bass", name: "Sub Bass", kind: "init-tone" },
    colorIndex: 3,
    volumeStep: 3,
    hot: false,
  },
  {
    track: "Track 5",
    patch: "Analog Brass",
    instrument: {
      id: "com.arturia.AnalogLab",
      name: "Analog Lab V",
      kind: "plugin",
      pluginFormat: "VST3",
      pluginId: "com.arturia.AnalogLab",
      pluginName: "Analog Lab V",
      pluginManufacturer: "Arturia",
      pluginVersion: "5.10.0",
    },
    colorIndex: 4,
    volumeStep: 4,
    hot: false,
  },
  {
    track: "Track 6",
    patch: "Vocal Chop",
    instrument: { id: "vocal-chop", name: "Vocal Chop", kind: "init-tone" },
    colorIndex: 5,
    volumeStep: 3,
    hot: false,
  },
  {
    track: "Track 7",
    patch: "Drum Kit",
    instrument: { id: "drum-kit", name: "Drum Kit", kind: "init-tone" },
    colorIndex: 6,
    volumeStep: 3,
    hot: false,
  },
  {
    track: "Track 8",
    patch: "Empty",
    instrument: { id: "", name: "Track 8", kind: "empty" },
    colorIndex: 7,
    volumeStep: 3,
    hot: false,
  },
];

function hotFx(index: number, assigned: boolean): ControlTrackHotFx[] {
  return [
    {
      index: 0,
      id: assigned ? "eq" : "",
      name: assigned ? "EQ" : "",
      bypassed: false,
    },
    {
      index: 1,
      id: assigned ? "compressor" : "",
      name: assigned ? "Compressor" : "",
      bypassed: index === 1,
    },
  ];
}

function stepDb(step: number): number {
  const gain = stepGain(step);

  return gain <= 0 ? -60 : 20 * Math.log10(gain);
}

function stepGain(step: number): number {
  return STEP_GAINS[step] ?? STEP_GAINS[3];
}

const TRACKS: ControlTrack[] = SPECS.map((spec, index) => ({
  index,
  id: `track-${String(index + 1)}`,
  name: spec.track,
  slot: index,
  selected: index === 2,
  instrument: spec.instrument,
  volumeStep: spec.volumeStep,
  volumeDb: stepDb(spec.volumeStep),
  gain: stepGain(spec.volumeStep),
  hotFx: hotFx(index, spec.hot),
}));

const FAMILIES: ControlState["families"] = SPECS.map((spec, index) => ({
  id: `track-${String(index + 1)}`,
  name: spec.track,
  patches: [
    {
      slot: index,
      id: `track-${String(index + 1)}`,
      name: spec.patch,
      family: `track-${String(index + 1)}`,
      familyName: spec.track,
      instrument: spec.instrument,
      volumeDb: 0,
      pan: 0,
      mute: false,
      solo: false,
      colorIndex: spec.colorIndex,
      warmState: index === 2 ? "active" : "warm",
      warm: true,
      editorOpen: false,
      latencySamples: index === 2 ? 240 : 0,
      latencyMs: index === 2 ? 5 : 0,
    },
  ],
}));

export const TRACK_FIXTURE: ControlState = {
  schemaVersion: 1,
  revision: 1,
  application: { name: "Instrument Hub", version: "0.1.0" },
  layout: "tracks",
  armedSlot: 2,
  uptimeMs: 128_000,
  slots: SPECS.map((spec, index) => ({
    index,
    name: spec.track,
    armed: index === 2,
    latencySamples: index === 2 ? 240 : 0,
    latencyMs: index === 2 ? 5 : 0,
    warmState: index === 2 ? "active" : "warm",
    error: "",
  })),
  tracks: TRACKS,
  families: FAMILIES,
  settings: {
    masterGain: 0.82,
    inputGain: 1.0,
    midiChannel: 2,
    metronome: true,
  },
  transport: { playing: true, bpm: 128, ppq: 16.5, positionSamples: 384_512 },
  project: {
    path: "/home/demo/Music/Instrument Hub/Tracks.ihub",
    name: "Tracks",
    dirty: false,
    saved: true,
    error: "",
  },
};
