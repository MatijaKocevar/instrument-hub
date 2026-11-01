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

/** Connection lifecycle for the polling loop. */
export type ConnectionPhase = "idle" | "connecting" | "ready" | "offline";

/** Immutable view of the store, read through `useSyncExternalStore`. */
export interface AppSnapshot
{
    phase: ConnectionPhase;
    /** Last successful `GET /state`, kept during reconnects. */
    state: ControlState | null;
    /** Last successful `GET /audio`, kept during reconnects. */
    audio: ControlAudioState | null;
    /** Message from the most recent state failure, cleared on success. */
    error: string | null;
    /** Message from the most recent audio-settings failure. */
    audioError: string | null;
    /** Last latency probe result. */
    measurement: ControlAudioMeasurement | null;
    /** Message from the most recent failed probe. */
    measurementError: string | null;
    /** True while a `POST /audio` is in flight. */
    audioBusy: boolean;
    /** Insert chain of `fxSlot`, or null before the first load. */
    fxChain: ControlFxChain | null;
    /** Slot `fxChain` describes; -1 when no chain is loaded. */
    fxSlot: number;
    /** Message from the most recent chain failure. */
    fxError: string | null;
    /** True while an insert request is in flight. */
    fxBusy: boolean;
    /** True while a source change is in flight. */
    sourceBusy: boolean;
    /** Slot whose native editor is open, or -1. */
    editorOpenSlot: number;
    /** True while an editor open/close request is in flight. */
    editorBusy: boolean;
    /** Message from the most recent editor failure. */
    editorError: string | null;
    /** True while an explicit patch-sound save is in flight. */
    captureBusy: boolean;
    /** Slot whose last explicit save succeeded, or -1. */
    captureSlot: number;
    /** Confirmation from the most recent explicit save, or null. */
    capture: ControlPatchStateCapture | null;
    /** Message from the most recent failed explicit save. */
    captureError: string | null;
    /** Installed plug-in inventory, or null before the first load. */
    plugins: ControlPluginList | null;
    /** Message from the most recent inventory failure. */
    pluginError: string | null;
    /** True while a scan or inventory request is in flight. */
    pluginBusy: boolean;
    /** MIDI inputs and controller status, or null before the first load. */
    midi: ControlMidiState | null;
    /** Message from the most recent MIDI failure. */
    midiError: string | null;
    /** True while a MIDI input toggle or reload is in flight. */
    midiBusy: boolean;
    /** True while the Launchpad pad test is starting. */
    padTestBusy: boolean;
    /** Message from the most recent pad test. */
    padTestError: string | null;
    /** Latest `GET /pairing`, or null when unavailable (e.g. on a tablet). */
    pairing: ControlPairingState | null;
    /** Message from the most recent pairing action. */
    pairingError: string | null;
    /** True while a LAN toggle, pair or forget request is in flight. */
    pairingBusy: boolean;
    /** True when the last state request was rejected with HTTP 401. */
    unauthorized: boolean;
    /** Consecutive failures; drives the exponential backoff. */
    failedAttempts: number;
    /** `Date.now()` of the last successful snapshot, `null` before the first. */
    updatedAt: number | null;
}

/** Snapshot before the first request; every flag starts idle and empty. */
export function initialSnapshot (): AppSnapshot
{
    return {
        phase: "idle",
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
