import type {
  ControlAudioMeasurement,
  ControlAudioState,
  ControlAudioWizard,
  ControlFxChain,
  ControlMidiState,
  ControlPairingState,
  ControlPatchStateCapture,
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
  measurementError: string | null;
  audioWizard: ControlAudioWizard | null;
  audioWizardBusy: boolean;
  audioWizardError: string | null;
  audioBusy: boolean;
  fxChain: ControlFxChain | null;
  fxSlot: number;
  fxError: string | null;
  fxBusy: boolean;
  sourceBusy: boolean;
  editorOpenSlot: number;
  editorBusy: boolean;
  editorError: string | null;
  editorErrorSlot: number;
  reloadBusy: boolean;
  reloadSlot: number;
  captureBusy: boolean;
  captureSlot: number;
  capture: ControlPatchStateCapture | null;
  captureError: string | null;
  plugins: ControlPluginList | null;
  pluginError: string | null;
  pluginBusy: boolean;
  midi: ControlMidiState | null;
  midiError: string | null;
  midiBusy: boolean;
  padTestBusy: boolean;
  padTestError: string | null;
  pairing: ControlPairingState | null;
  pairingError: string | null;
  pairingBusy: boolean;
  project: ControlProjectOverview | null;
  projectError: string | null;
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
    measurementError: null,
    audioWizard: null,
    audioWizardBusy: false,
    audioWizardError: null,
    audioBusy: false,
    fxChain: null,
    fxSlot: -1,
    fxError: null,
    fxBusy: false,
    sourceBusy: false,
    editorOpenSlot: -1,
    editorBusy: false,
    editorError: null,
    editorErrorSlot: -1,
    reloadBusy: false,
    reloadSlot: -1,
    captureBusy: false,
    captureSlot: -1,
    capture: null,
    captureError: null,
    plugins: null,
    pluginError: null,
    pluginBusy: false,
    midi: null,
    midiError: null,
    midiBusy: false,
    padTestBusy: false,
    padTestError: null,
    pairing: null,
    pairingError: null,
    pairingBusy: false,
    project: null,
    projectError: null,
    projectBusy: false,
    unauthorized: false,
    failedAttempts: 0,
    updatedAt: null,
  };
}
