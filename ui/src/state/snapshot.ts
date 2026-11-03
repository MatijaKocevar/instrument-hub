import type {
  ControlAudioMeasurement,
  ControlAudioState,
  ControlFxChain,
  ControlMidiState,
  ControlPairingState,
  ControlPatchStateCapture,
  ControlPluginList,
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
  audioBusy: boolean;
  fxChain: ControlFxChain | null;
  fxSlot: number;
  fxError: string | null;
  fxBusy: boolean;
  sourceBusy: boolean;
  editorOpenSlot: number;
  editorBusy: boolean;
  editorError: string | null;
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
    audioBusy: false,
    fxChain: null,
    fxSlot: -1,
    fxError: null,
    fxBusy: false,
    sourceBusy: false,
    editorOpenSlot: -1,
    editorBusy: false,
    editorError: null,
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
    unauthorized: false,
    failedAttempts: 0,
    updatedAt: null,
  };
}
