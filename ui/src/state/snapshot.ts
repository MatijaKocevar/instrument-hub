import type {
  ControlAudioMeasurement,
  ControlAudioState,
  ControlAudioWizard,
  ControlFxChain,
  ControlMidiState,
  ControlPairingState,
  ControlPluginList,
  ControlProjectOverview,
  ControlState,
} from "../api/client";

export type ConnectionPhase = "idle" | "connecting" | "ready" | "offline";

export interface AppSnapshot {
  phase: ConnectionPhase;
  live: boolean;
  state: ControlState | null;
  audio: ControlAudioState | null;
  error: string | null;
  audioError: string | null;
  measurement: ControlAudioMeasurement | null;
  audioWizard: ControlAudioWizard | null;
  audioWizardBusy: boolean;
  audioBusy: boolean;
  fxChain: ControlFxChain | null;
  fxSlot: number;
  fxError: string | null;
  fxBusy: boolean;
  sourceBusy: boolean;
  editorBusy: boolean;
  reloadBusy: boolean;
  reloadSlot: number;
  captureBusy: boolean;
  plugins: ControlPluginList | null;
  pluginError: string | null;
  pluginBusy: boolean;
  midi: ControlMidiState | null;
  midiError: string | null;
  midiBusy: boolean;
  pairing: ControlPairingState | null;
  pairingBusy: boolean;
  project: ControlProjectOverview | null;
  projectBusy: boolean;
  unauthorized: boolean;
  failedAttempts: number;
  updatedAt: number | null;
}

export function initialSnapshot(): AppSnapshot {
  return {
    phase: "idle",
    live: false,
    state: null,
    audio: null,
    error: null,
    audioError: null,
    measurement: null,
    audioWizard: null,
    audioWizardBusy: false,
    audioBusy: false,
    fxChain: null,
    fxSlot: -1,
    fxError: null,
    fxBusy: false,
    sourceBusy: false,
    editorBusy: false,
    reloadBusy: false,
    reloadSlot: -1,
    captureBusy: false,
    plugins: null,
    pluginError: null,
    pluginBusy: false,
    midi: null,
    midiError: null,
    midiBusy: false,
    pairing: null,
    pairingBusy: false,
    project: null,
    projectBusy: false,
    unauthorized: false,
    failedAttempts: 0,
    updatedAt: null,
  };
}
