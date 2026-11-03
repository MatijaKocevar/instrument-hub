import type { ControlState } from "../../api/client";

type FixturePatch = ControlState["families"][number]["patches"][number];

const FIXTURE_FAMILIES: readonly {
  id: string;
  name: string;
  patches: readonly [string, string][];
}[] = [
  {
    id: "keys",
    name: "Keys",
    patches: [
      ["piano", "Studio Grand"],
      ["strings", "Warm Pad"],
      ["rhodes", "Electric Keys"],
      ["organ", "Drawbar Organ"],
      ["harp", "Concert Harp"],
      ["choir", "Choir Keys"],
      ["celesta", "Celesta"],
      ["clav", "Clavinet"],
    ],
  },
  {
    id: "drums",
    name: "Drums",
    patches: [
      ["kit", "Acoustic Kit"],
      ["snare", "Snare Stack"],
      ["hihats", "Hi-Hats"],
      ["toms", "Toms"],
      ["clap", "Clap"],
      ["rimshot", "Rimshot"],
      ["cymbals", "Cymbals"],
      ["kick", "Kick"],
    ],
  },
  {
    id: "bass",
    name: "Bass",
    patches: [
      ["sub", "Sub Bass"],
      ["finger", "Finger Bass"],
      ["pick", "Pick Bass"],
      ["fretless", "Fretless Bass"],
      ["synth", "Synth Bass"],
      ["upright", "Upright Bass"],
      ["growl", "Growl Bass"],
      ["octave", "Octave Bass"],
    ],
  },
  {
    id: "pads",
    name: "Pads",
    patches: [
      ["air", "Air Pad"],
      ["warm", "Warm Pad"],
      ["glass", "Glass Pad"],
      ["choir", "Choir Pad"],
      ["motion", "Motion Pad"],
      ["dark", "Dark Pad"],
      ["shimmer", "Shimmer"],
      ["tape", "Tape Pad"],
    ],
  },
  {
    id: "lead",
    name: "Lead",
    patches: [
      ["saw", "Saw Lead"],
      ["square", "Square Lead"],
      ["sine", "Sine Lead"],
      ["sync", "Sync Lead"],
      ["fifth", "Fifth Lead"],
      ["soft", "Soft Lead"],
      ["hard", "Hard Lead"],
      ["pulse", "Pulse Lead"],
    ],
  },
  {
    id: "fx",
    name: "FX",
    patches: [
      ["riser", "Riser"],
      ["down", "Downlifter"],
      ["impact", "Impact"],
      ["noise", "Noise Sweep"],
      ["sweep", "Sweep"],
      ["reverse", "Reverse"],
      ["texture", "Texture"],
      ["drone", "Drone"],
    ],
  },
  {
    id: "perc",
    name: "Percussion",
    patches: [
      ["shaker", "Shaker"],
      ["tamb", "Tambourine"],
      ["conga", "Conga"],
      ["bongo", "Bongo"],
      ["cowbell", "Cowbell"],
      ["clave", "Clave"],
      ["woodblock", "Woodblock"],
      ["triangle", "Triangle"],
    ],
  },
  {
    id: "vox",
    name: "Vox",
    patches: [
      ["chop", "Vocal Chop"],
      ["ooh", "Vocal Ooh"],
      ["aah", "Vocal Aah"],
      ["stack", "Vocal Stack"],
      ["phrase", "Vocal Phrase"],
      ["whisper", "Whisper"],
      ["shout", "Shout"],
      ["robot", "Robot Vox"],
    ],
  },
];

let fixtureSlot = 0;

const FIXTURE_PATCHES: ControlState["families"] = FIXTURE_FAMILIES.map(
  (family, familyIndex) => ({
    id: family.id,
    name: family.name,
    patches: family.patches.map(([id, name]): FixturePatch => {
      const slot = fixtureSlot;
      fixtureSlot += 1;

      return {
        slot,
        id,
        name,
        family: family.id,
        familyName: family.name,
        instrument:
          slot === 6
            ? {
                id: "com.arturia.AnalogLab",
                name: "Analog Lab V",
                kind: "plugin",
                pluginFormat: "VST3",
                pluginId: "com.arturia.AnalogLab",
                pluginName: "Analog Lab V",
                pluginManufacturer: "Arturia",
                pluginVersion: "5.10.0",
              }
            : { id, name, kind: "init-tone" },
        volumeDb: 0,
        pan: 0,
        mute: false,
        solo: false,
        colorIndex: familyIndex,
        warmState: slot === 3 ? "active" : "warm",
        warm: true,
        editorOpen: false,
        latencySamples: slot === 3 ? 240 : 0,
        latencyMs: slot === 3 ? 5 : 0,
      };
    }),
  }),
);

export const FIXTURE: ControlState = {
  schemaVersion: 1,
  revision: 1,
  application: { name: "Instrument Hub", version: "0.1.0" },
  armedSlot: 3,
  uptimeMs: 128_000,
  slots: [
    {
      index: 0,
      name: "Studio Grand",
      armed: false,
      latencySamples: 0,
      latencyMs: 0,
    },
    {
      index: 1,
      name: "Warm Pad",
      armed: false,
      latencySamples: 72,
      latencyMs: 1.5,
    },
    {
      index: 2,
      name: "Electric Keys",
      armed: false,
      latencySamples: 0,
      latencyMs: 0,
    },
    {
      index: 3,
      name: "Sub Bass",
      armed: true,
      latencySamples: 240,
      latencyMs: 5,
    },
    {
      index: 4,
      name: "Drum Kit",
      armed: false,
      latencySamples: 120,
      latencyMs: 2.5,
    },
    {
      index: 5,
      name: "Analog Brass",
      armed: false,
      latencySamples: 0,
      latencyMs: 0,
    },
    {
      index: 6,
      name: "Vocal Chop",
      armed: false,
      latencySamples: 96,
      latencyMs: 2,
    },
    {
      index: 7,
      name: "Empty Slot",
      armed: false,
      latencySamples: 0,
      latencyMs: 0,
    },
  ],
  families: FIXTURE_PATCHES,
  settings: {
    masterGain: 0.82,
    inputGain: 1.0,
    midiChannel: 2,
    metronome: true,
  },
  transport: { playing: true, bpm: 128, ppq: 16.5, positionSamples: 384_512 },
};
