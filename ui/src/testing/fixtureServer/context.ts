import type {
  ControlAudioState,
  ControlFxChain,
  ControlMidiState,
  ControlState,
} from "../../api/client";
import {
  AUDIO,
  CHAIN_INSTRUMENT_LATENCY,
  COMPRESSOR_LATENCY,
  FIXTURE,
  MIDI,
  PAIRING_OFF,
  PLUGIN_FIXTURE,
  TRACK_FIXTURE,
  fixtureInsert,
  recalculateChain,
} from "../fixtures";
import type { FixtureContext, FixtureOptions } from "./types";

function cloneState(state: ControlState): ControlState {
  return JSON.parse(JSON.stringify(state)) as ControlState;
}

function cloneAudio(audio: ControlAudioState): ControlAudioState {
  return JSON.parse(JSON.stringify(audio)) as ControlAudioState;
}

function cloneMidi(midi: ControlMidiState): ControlMidiState {
  return JSON.parse(JSON.stringify(midi)) as ControlMidiState;
}

export function createFixtureContext(
  options: FixtureOptions = {},
): FixtureContext {
  const chains = new Map<number, ControlFxChain>();

  const chainFor = (slot: number): ControlFxChain => {
    const existing = chains.get(slot);

    if (existing !== undefined) return existing;

    const hasInserts = options.layout === "tracks" || slot === 3;

    const chain: ControlFxChain = {
      slot,
      instrumentLatencySamples:
        options.layout === "tracks"
          ? slot === 2
            ? CHAIN_INSTRUMENT_LATENCY
            : 0
          : slot === 3
            ? CHAIN_INSTRUMENT_LATENCY
            : 0,
      fxLatencySamples: 0,
      latencySamples: 0,
      latencyMs: 0,
      inserts: hasInserts
        ? [
            fixtureInsert(0, "eq", "eq", "EQ", 0, {
              lowGainDb: 2,
              midGainDb: 0,
              highGainDb: -1,
            }),
            fixtureInsert(
              1,
              "compressor",
              "compressor",
              "Compressor",
              COMPRESSOR_LATENCY,
              { thresholdDb: -24, ratio: 3 },
            ),
          ]
        : [],
    };

    recalculateChain(chain);
    chains.set(slot, chain);
    return chain;
  };

  return {
    failFirst: options.failFirst ?? 0,
    armDelayMs: options.armDelayMs ?? 0,
    state: cloneState(options.layout === "tracks" ? TRACK_FIXTURE : FIXTURE),
    audio: cloneAudio(AUDIO),
    midi: cloneMidi(MIDI),
    chains,
    project: {
      path: "/home/demo/Music/Instrument Hub/Demo.ihub",
      name: "Demo",
      dirty: false,
      saved: true,
      error: "",
      recent: [
        { path: "/home/demo/Music/Instrument Hub/Demo.ihub", name: "Demo" },
        {
          path: "/home/demo/Music/Instrument Hub/Rehearsal.ihub",
          name: "Rehearsal",
        },
      ],
    },
    pairing: { ...PAIRING_OFF },
    pairingToken: null,
    pluginPlugins: PLUGIN_FIXTURE.plugins.map((plugin) => ({ ...plugin })),
    pluginScanning: false,
    pluginPendingScan: false,
    stateRequests: 0,
    measureRequests: 0,
    wizardRequests: 0,
    wizardApplyRequests: 0,
    padTestRequests: 0,
    armRequests: [],
    trackRequests: [],
    audioRequests: [],
    transportRequests: [],
    pairRequests: [],
    lanRequests: [],
    patchRequests: [],
    fxRequests: [],
    sourceRequests: [],
    editorRequests: [],
    reloadRequests: [],
    stateCaptureRequests: [],
    pluginRequests: [],
    midiRequests: [],
    projectRequests: [],
    chainFor,
  };
}

export function findPatch(
  context: FixtureContext,
  slot: number,
): ControlState["families"][number]["patches"][number] | null {
  for (const family of context.state.families)
    for (const patch of family.patches) if (patch.slot === slot) return patch;

  return null;
}
