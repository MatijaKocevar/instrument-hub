import type {
    ControlApi,
    ControlAudioPatch,
    ControlFxInsertRequest,
    ControlFxInsertUpdate,
    ControlPatchSourceRequest,
    ControlPatchUpdate,
    ControlState,
    ControlTransportPatch,
} from "../api/client";
import type { ActionContext } from "./actionContext";
import { AudioActions } from "./audio";
import { FxActions } from "./fx";
import { MidiActions } from "./midi";
import {
    resolveAppStoreOptions,
    type AppStoreOptions,
    type ResolvedAppStoreOptions,
} from "./options";
import { PairingActions } from "./pairing";
import { PatchArmActions } from "./patchArm";
import { PatchCaptureActions } from "./patchCapture";
import { PatchEditorActions } from "./patchEditor";
import { PatchSourceActions } from "./patchSource";
import { PatchUpdateActions } from "./patchUpdate";
import { PluginActions } from "./plugins";
import { PollLoop } from "./pollLoop";
import { refreshSnapshot } from "./refresh";
import type { AppSnapshot } from "./snapshot";
import { StoreCore } from "./storeCore";
import { TransportActions } from "./transport";

export type { AppStoreOptions } from "./options";
export type { AppSnapshot, ConnectionPhase } from "./snapshot";

/**
    Holds the control state and keeps it fresh.

    A single polling loop serialises requests: `refresh()` de-duplicates while a
    request is in flight, so a slow engine can never stack requests. Failures
    back off exponentially (250 ms doubling to 5 s) and the last good state
    stays visible, which keeps the UI usable across an engine restart.

    Domain actions live in their own modules, one per API area: patches and FX
    in `patch*.ts` and `fx.ts`, inventory in `plugins.ts`, inputs in `midi.ts`,
    settings in `audio.ts`, transport in `transport.ts` and pairing in
    `pairing.ts`. This class keeps lifecycle and the public façade.

    Nothing here touches the audio thread — this is pure UI state.
*/
export class AppStore
{
    private readonly api: ControlApi;
    private readonly options: ResolvedAppStoreOptions;
    private readonly core: StoreCore;
    private readonly polling: PollLoop;
    private readonly patchArm: PatchArmActions;
    private readonly patchUpdate: PatchUpdateActions;
    private readonly patchSource: PatchSourceActions;
    private readonly patchEditor: PatchEditorActions;
    private readonly patchCapture: PatchCaptureActions;
    private readonly fx: FxActions;
    private readonly plugins: PluginActions;
    private readonly midi: MidiActions;
    private readonly audio: AudioActions;
    private readonly transport: TransportActions;
    private readonly pairing: PairingActions;

    constructor (api: ControlApi, options: AppStoreOptions = {})
    {
        this.api = api;
        this.options = resolveAppStoreOptions (options);
        this.core = new StoreCore();

        const context: ActionContext = {
            api,
            now: this.options.now,
            getSnapshot: this.core.getSnapshot,
            patch: this.core.patch,
        };

        this.patchArm = new PatchArmActions (context);
        this.patchUpdate = new PatchUpdateActions (context);
        this.fx = new FxActions (context);
        this.patchSource = new PatchSourceActions (
            context, slot => this.fx.loadFxChain (slot, { force: true }));
        this.patchEditor = new PatchEditorActions (context);
        this.patchCapture = new PatchCaptureActions (context);
        this.plugins = new PluginActions (context);
        this.midi = new MidiActions (context);
        this.audio = new AudioActions (context);
        this.transport = new TransportActions (context, slot => this.patchArm.armSlot (slot));
        this.pairing = new PairingActions (context, {
            storage: this.options.storage,
            refresh: () => this.refresh(),
        });

        this.polling = new PollLoop (
            this.options,
            () => this.load(),
            () => this.core.getSnapshot ().failedAttempts);
    }

    getSnapshot = (): AppSnapshot => this.core.getSnapshot ();

    /** Origin of the control server, shown in the connection strip. */
    get baseUrl (): string
    {
        return this.api.baseUrl;
    }

    /** Token this browser sends, or null when unpaired. */
    get token (): string | null
    {
        return this.api.token;
    }

    subscribe = (listener: () => void): (() => void) => this.core.subscribe (listener);

    /**
        Fetch the state once.

        Resolves `true` on success and `false` on failure; concurrent callers
        share one request.
    */
    refresh = (): Promise<boolean> => this.polling.refresh ();

    /** Begin polling. Safe to call more than once. */
    start (): void
    {
        this.polling.start ();
    }

    /** Stop polling and drop any pending timer. */
    stop (): void
    {
        this.polling.stop ();
    }

    /** Delay before the next attempt after `failedAttempts` consecutive misses. */
    retryDelayMs (): number
    {
        return this.polling.retryDelayMs ();
    }

    //==========================================================================
    // Patches (grid arming, mix, colour, source, editor, capture)

    armSlot = (slot: number | null): Promise<boolean> => this.patchArm.armSlot (slot);

    updatePatch = (slot: number, patch: ControlPatchUpdate): Promise<boolean> =>
        this.patchUpdate.updatePatch (slot, patch);

    updateFamilyColour = (familyId: string, colorIndex: number): Promise<boolean> =>
        this.patchUpdate.updateFamilyColour (familyId, colorIndex);

    changeInstrumentSource = (slot: number,
                              source: ControlPatchSourceRequest): Promise<boolean> =>
        this.patchSource.changeInstrumentSource (slot, source);

    openInstrumentEditor = (slot: number): Promise<boolean> =>
        this.patchEditor.openInstrumentEditor (slot);

    closeInstrumentEditor = (slot: number): Promise<boolean> =>
        this.patchEditor.closeInstrumentEditor (slot);

    savePatchSound = (slot: number): Promise<boolean> =>
        this.patchCapture.savePatchSound (slot);

    //==========================================================================
    // Per-patch insert chain (T121)

    loadFxChain = (slot: number, options: { force?: boolean } = {}): Promise<boolean> =>
        this.fx.loadFxChain (slot, options);

    addFxInsert = (slot: number, insert: ControlFxInsertRequest): Promise<boolean> =>
        this.fx.addFxInsert (slot, insert);

    updateFxInsert = (slot: number,
                      index: number,
                      update: ControlFxInsertUpdate): Promise<boolean> =>
        this.fx.updateFxInsert (slot, index, update);

    removeFxInsert = (slot: number, index: number): Promise<boolean> =>
        this.fx.removeFxInsert (slot, index);

    moveFxInsert = (slot: number, from: number, to: number): Promise<boolean> =>
        this.fx.moveFxInsert (slot, from, to);

    replaceFxChain = (slot: number,
                      inserts: readonly ControlFxInsertRequest[]): Promise<boolean> =>
        this.fx.replaceFxChain (slot, inserts);

    //==========================================================================
    // Plug-in inventory (T122)

    loadPlugins = (search?: string): Promise<boolean> => this.plugins.loadPlugins (search);

    rescanPlugins = (): Promise<boolean> => this.plugins.rescanPlugins ();

    //==========================================================================
    // MIDI inputs and the pad test (T126)

    loadMidi = (): Promise<boolean> => this.midi.loadMidi ();

    toggleMidiInput = (id: string, enabled: boolean): Promise<boolean> =>
        this.midi.toggleMidiInput (id, enabled);

    runPadTest = (): Promise<boolean> => this.midi.runPadTest ();

    //==========================================================================
    // Transport

    updateTransport = (patch: ControlTransportPatch): Promise<boolean> =>
        this.transport.updateTransport (patch);

    panic = (): Promise<boolean> => this.transport.panic ();

    //==========================================================================
    // Audio settings (T124)

    updateAudio = (patch: ControlAudioPatch): Promise<boolean> =>
        this.audio.updateAudio (patch);

    measureLatency = (): Promise<boolean> => this.audio.measureLatency ();

    //==========================================================================
    // LAN pairing (T101)

    toggleLan = (enabled: boolean): Promise<boolean> => this.pairing.toggleLan (enabled);

    pair = (code: string): Promise<boolean> => this.pairing.pair (code);

    forgetPairing = (): Promise<boolean> => this.pairing.forgetPairing ();

    private load (): Promise<boolean>
    {
        return refreshSnapshot ({
            api: this.api,
            now: this.options.now,
            getSnapshot: this.core.getSnapshot,
            patch: this.core.patch,
            preservePending: state => this.withPendingState (state),
        });
    }

    /**
        Keep optimistic changes made by requests that are still in flight.

        A poll can land before an arm, transport or patch request answers; its
        snapshot still describes the previous value. Re-applying the confirmed
        snapshot's optimistic field keeps the UI from snapping back until the
        action itself confirms or rolls back.
    */
    private withPendingState (state: ControlState): ControlState
    {
        let merged = this.patchArm.preservePending (state);

        merged = this.patchUpdate.preservePending (merged);

        return this.transport.preservePending (merged);
    }
}
