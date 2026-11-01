import type {
    ControlApi,
    ControlAudioPatch,
    ControlFxChain,
    ControlFxInsertRequest,
    ControlFxInsertUpdate,
    ControlPatchSourceRequest,
    ControlPatchUpdate,
    ControlState,
    ControlTransportPatch,
} from "../api/client";
import { ApiError } from "../api/client";
import {
    chainWithBypass,
    chainWithMove,
    chainWithoutInsert,
    moveOrder,
} from "../features/instrument/model";
import {
    clearPairingToken,
    isPairingCode,
    savePairingToken,
} from "../features/pairing/model";
import { messageOf } from "./errors";
import {
    resolveAppStoreOptions,
    type AppStoreOptions,
    type ResolvedAppStoreOptions,
} from "./options";
import { PollLoop } from "./pollLoop";
import { refreshSnapshot } from "./refresh";
import type { AppSnapshot } from "./snapshot";
import { StoreCore } from "./storeCore";

export type { AppStoreOptions } from "./options";
export type { AppSnapshot, ConnectionPhase } from "./snapshot";

/**
    Return `state` with `slot` armed.

    Used for the optimistic highlight: the grid reads `armedSlot`, so patching
    this in the same tick as the click makes the highlight move on the next
    render instead of after the next 2 s poll.
*/
function withArmedSlot (state: ControlState, slot: number): ControlState
{
    return {
        ...state,
        armedSlot: slot,
        slots: state.slots.map (candidate => ({
            ...candidate,
            armed: candidate.index === slot,
        })),
    };
}

/**
    Return `state` with a partial patch update applied to `slot`.

    Used for the same optimistic highlight rule as arming: a colour swatch
    moves the cell on the next render even if the engine round trip is slow.
*/
function withPatchUpdate (state: ControlState,
                          slot: number,
                          patch: ControlPatchUpdate): ControlState
{
    return {
        ...state,
        families: state.families.map (family => ({
            ...family,
            patches: family.patches.map (candidate => candidate.slot === slot
                ? { ...candidate, ...patch }
                : candidate),
        })),
    };
}

/** Return `state` with one patch's `editorOpen` flag updated. */
function withEditorOpen (state: ControlState, slot: number, open: boolean): ControlState
{
    return {
        ...state,
        families: state.families.map (family => ({
            ...family,
            patches: family.patches.map (candidate => candidate.slot === slot
                ? { ...candidate, editorOpen: open }
                : candidate),
        })),
    };
}

/**
    Holds the control state and keeps it fresh.

    A single polling loop serialises requests: `refresh()` de-duplicates while a
    request is in flight, so a slow engine can never stack requests. Failures
    back off exponentially (250 ms doubling to 5 s) and the last good state
    stays visible, which keeps the UI usable across an engine restart.

    Nothing here touches the audio thread — this is pure UI state.
*/
export class AppStore
{
    private readonly api: ControlApi;
    private readonly options: ResolvedAppStoreOptions;
    private readonly core: StoreCore;
    private readonly polling: PollLoop;

    /** Sequence of arm requests; a stale response must not win. */
    private armSequence = 0;
    /** Number of arm requests in flight, used to keep the highlight during polls. */
    private armInFlight = 0;
    /** Sequence of patch-colour requests; a stale response must not win. */
    private patchSequence = 0;
    /** Number of patch requests in flight, used to keep the fader during polls. */
    private patchInFlight = 0;
    /** Latest optimistic patch while a request is in flight. */
    private pendingPatch: { slot: number; patch: ControlPatchUpdate } | null = null;
    /** Sequence of insert-chain requests; a stale response must not win. */
    private fxSequence = 0;
    /** Sequence of transport requests; a stale response must not win. */
    private transportSequence = 0;
    /** Number of transport requests in flight, used to keep Play/Pause during polls. */
    private transportInFlight = 0;

    constructor (api: ControlApi, options: AppStoreOptions = {})
    {
        this.api = api;
        this.options = resolveAppStoreOptions (options);
        this.core = new StoreCore();
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

    /**
        Arm `slot` (or disarm with `null`).

        The store applies the highlight optimistically, synchronously, before
        the request leaves. A failed request rolls the highlight back to the
        last confirmed state. Returns `false` without a request when the slot
        is outside the engine's slot list.
    */
    armSlot = async (slot: number | null): Promise<boolean> =>
    {
        const target = slot === null ? -1 : slot;
        const previous = this.snapshot.state;

        if (previous !== null && target >= previous.slots.length)
            return false;

        const sequence = this.armSequence + 1;
        this.armSequence = sequence;
        this.armInFlight += 1;

        if (previous !== null)
            this.patch ({ state: withArmedSlot (previous, target), error: null });

        try
        {
            const state = await this.api.armSlot (slot);

            if (sequence === this.armSequence)
            {
                this.patch ({
                    phase: "ready",
                    state,
                    error: null,
                    failedAttempts: 0,
                    updatedAt: this.options.now(),
                });
            }

            return true;
        }
        catch (error)
        {
            if (sequence === this.armSequence)
            {
                this.patch ({
                    error: messageOf (error),
                    ...(previous === null ? {} : { state: previous }),
                });
            }

            return false;
        }
        finally
        {
            this.armInFlight -= 1;
        }
    };

    /**
        Change one patch's mix or colour.

        The store applies the change optimistically (so the cell recolours on
        the next render) and adopts the engine's complete state when the
        request answers. A rejection restores the last confirmed state.
    */
    updatePatch = async (slot: number, patch: ControlPatchUpdate): Promise<boolean> =>
    {
        const previous = this.snapshot.state;

        if (previous === null)
            return false;

        const sequence = this.patchSequence + 1;
        this.patchSequence = sequence;
        this.patchInFlight += 1;
        this.pendingPatch = { slot, patch };

        this.patch ({ state: withPatchUpdate (previous, slot, patch), error: null });

        try
        {
            const state = await this.api.updatePatch (slot, patch);

            if (sequence === this.patchSequence)
            {
                this.patch ({
                    phase: "ready",
                    state,
                    error: null,
                    failedAttempts: 0,
                    updatedAt: this.options.now(),
                });
            }

            return true;
        }
        catch (error)
        {
            if (sequence === this.patchSequence)
                this.patch ({ error: messageOf (error), state: previous });

            return false;
        }
        finally
        {
            this.patchInFlight -= 1;

            if (this.patchInFlight === 0 && sequence === this.patchSequence)
                this.pendingPatch = null;
        }
    };

    /**
        Recolour every patch in a family.

        The API stores colour per patch, so the family default is applied as
        one PATCH per patch. Sequential requests keep the final state
        deterministic if one is rejected.
    */
    updateFamilyColour = async (familyId: string, colorIndex: number): Promise<boolean> =>
    {
        const family = this.snapshot.state?.families.find (candidate => candidate.id === familyId);

        if (family === undefined)
            return false;

        for (const patch of family.patches)
        {
            if (! await this.updatePatch (patch.slot, { colorIndex }))
                return false;
        }

        return true;
    };

    //==========================================================================
    // Per-patch insert chain (T121) and instrument source (T124)

    /**
        Load one patch's insert chain.

        The page calls this when its slot changes. A chain already loaded for
        the same slot is reused unless `force` is set, so re-rendering the page
        never spams the engine.
    */
    loadFxChain = async (slot: number, options: { force?: boolean } = {}): Promise<boolean> =>
    {
        if (! Number.isInteger (slot) || slot < 0)
            return false;

        if (options.force !== true && this.snapshot.fxSlot === slot && this.snapshot.fxChain !== null)
            return true;

        this.patch ({ fxBusy: true, fxError: null });

        try
        {
            const chain = await this.api.getFxChain (slot);

            this.patch ({ fxChain: chain, fxSlot: slot, fxBusy: false, fxError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ fxBusy: false, fxError: messageOf (error) });
            return false;
        }
    };

    /** Append an insert (built-in or VST3) to the patch's chain. */
    addFxInsert = async (slot: number, insert: ControlFxInsertRequest): Promise<boolean> =>
        this.runFx (slot, () => this.api.addFxInsert (slot, insert), null);

    /** Bypass, re-balance or re-parameterise one insert. */
    updateFxInsert = async (slot: number,
                            index: number,
                            update: ControlFxInsertUpdate): Promise<boolean> =>
        this.runFx (
            slot,
            () => this.api.updateFxInsert (slot, index, update),
            update.bypassed === undefined
                ? null
                : this.optimisticChain (slot, chain => chainWithBypass (chain, index, update.bypassed === true)));

    /** Remove one insert, dropping it from the page on the next render. */
    removeFxInsert = async (slot: number, index: number): Promise<boolean> =>
        this.runFx (
            slot,
            () => this.api.removeFxInsert (slot, index),
            this.optimisticChain (slot, chain => chainWithoutInsert (chain, index)));

    /** Move one insert `from` -> `to`; the engine answers with the new order. */
    moveFxInsert = async (slot: number, from: number, to: number): Promise<boolean> =>
    {
        const chain = this.snapshot.fxSlot === slot ? this.snapshot.fxChain : null;

        if (chain === null)
            return false;

        const order = moveOrder (chain.inserts.length, from, to);

        return this.runFx (
            slot,
            () => this.api.reorderFxChain (slot, order),
            chainWithMove (chain, from, to));
    };

    /** Replace the chain with a played sequence, used by reload and tests. */
    replaceFxChain = async (slot: number,
                            inserts: readonly ControlFxInsertRequest[]): Promise<boolean> =>
        this.runFx (slot, () => this.api.replaceFxChain (slot, inserts), null);

    /**
        Swap the patch's instrument source.

        The engine re-binds the slot to a freshly instantiated source, so the
        complete state is adopted and the chain is re-read for the new latency
        budget.
    */
    changeInstrumentSource = async (slot: number,
                                    source: ControlPatchSourceRequest): Promise<boolean> =>
    {
        this.patch ({ sourceBusy: true, error: null });

        try
        {
            const state = await this.api.changePatchSource (slot, source);

            this.patch ({
                phase: "ready",
                state,
                sourceBusy: false,
                editorOpenSlot: -1,
                editorError: null,
                captureBusy: false,
                captureSlot: -1,
                capture: null,
                captureError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.options.now(),
            });

            await this.loadFxChain (slot, { force: true });
            return true;
        }
        catch (error)
        {
            this.patch ({ sourceBusy: false, error: messageOf (error) });
            return false;
        }
    };

    /**
        Open the patch's native plugin editor.

        Only this explicit action creates a window; selecting or arming a patch
        never does. The engine answers with `{ slot, open, reused }`, and the
        page's patch is updated in place so the button flips immediately.
    */
    openInstrumentEditor = async (slot: number): Promise<boolean> =>
    {
        this.patch ({ editorBusy: true, editorError: null });

        try
        {
            const result = await this.api.openPatchEditor (slot);
            const current = this.snapshot.state;

            this.patch ({
                phase: "ready",
                state: current === null ? null : withEditorOpen (current, slot, result.open),
                editorOpenSlot: result.open ? slot : -1,
                editorBusy: false,
                editorError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.options.now(),
            });

            return true;
        }
        catch (error)
        {
            this.patch ({ editorBusy: false, editorError: messageOf (error) });
            return false;
        }
    };

    /** Close the patch's native editor. Idempotent. */
    closeInstrumentEditor = async (slot: number): Promise<boolean> =>
    {
        this.patch ({ editorBusy: true, editorError: null });

        try
        {
            const result = await this.api.closePatchEditor (slot);
            const current = this.snapshot.state;

            this.patch ({
                phase: "ready",
                state: current === null ? null : withEditorOpen (current, slot, result.open),
                editorOpenSlot: result.open ? slot : -1,
                editorBusy: false,
                editorError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.options.now(),
            });

            return true;
        }
        catch (error)
        {
            this.patch ({ editorBusy: false, editorError: messageOf (error) });
            return false;
        }
    };

    /**
        Save the running plugin's sound into its patch.

        This is the explicit counterpart to the automatic capture on switch,
        editor close and eviction: a tweak can be kept without changing
        patches. The engine writes the sidecar blob before it answers, so the
        confirmation already describes persistent state.
    */
    savePatchSound = async (slot: number): Promise<boolean> =>
    {
        this.patch ({
            captureBusy: true,
            captureSlot: -1,
            capture: null,
            captureError: null,
        });

        try
        {
            const capture = await this.api.capturePatchState (slot);

            this.patch ({
                phase: "ready",
                captureBusy: false,
                captureSlot: slot,
                capture,
                captureError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.options.now(),
            });

            return true;
        }
        catch (error)
        {
            this.patch ({
                captureBusy: false,
                captureSlot: slot,
                capture: null,
                captureError: messageOf (error),
            });

            return false;
        }
    };

    //==========================================================================
    // Plug-in inventory (T122)

    /** Read the installed VST3 inventory, optionally filtered by `search`. */
    loadPlugins = async (search?: string): Promise<boolean> =>
    {
        this.patch ({ pluginBusy: true, pluginError: null });

        try
        {
            const plugins = await this.api.listPlugins (
                search === undefined || search.length === 0 ? {} : { search });

            this.patch ({ plugins, pluginBusy: false, pluginError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ pluginBusy: false, pluginError: messageOf (error) });
            return false;
        }
    };

    /** Ask the engine to rescan the VST3 directories. */
    rescanPlugins = async (): Promise<boolean> =>
    {
        this.patch ({ pluginBusy: true, pluginError: null });

        try
        {
            const plugins = await this.api.rescanPlugins ();

            this.patch ({ plugins, pluginBusy: false, pluginError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ pluginBusy: false, pluginError: messageOf (error) });
            return false;
        }
    };

    //==========================================================================
    // MIDI inputs and the pad test (T126)

    /** Read the ALSA inputs and controller status behind `GET /midi`. */
    loadMidi = async (): Promise<boolean> =>
    {
        this.patch ({ midiBusy: true, midiError: null });

        try
        {
            const midi = await this.api.getMidi ();

            this.patch ({ midi, midiBusy: false, midiError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ midiBusy: false, midiError: messageOf (error) });
            return false;
        }
    };

    /** Open or close one MIDI input. */
    toggleMidiInput = async (id: string, enabled: boolean): Promise<boolean> =>
    {
        this.patch ({ midiBusy: true, midiError: null });

        try
        {
            const midi = await this.api.setMidiInput (id, enabled);

            this.patch ({ midi, midiBusy: false, midiError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ midiBusy: false, midiError: messageOf (error) });
            return false;
        }
    };

    /** Light the Launchpad grid for the pad-test animation. */
    runPadTest = async (): Promise<boolean> =>
    {
        this.patch ({ padTestBusy: true, padTestError: null });

        try
        {
            const midi = await this.api.runMidiPadTest ();

            this.patch ({ midi, padTestBusy: false, padTestError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ padTestBusy: false, padTestError: messageOf (error) });
            return false;
        }
    };

    private optimisticChain (slot: number,
                             change: (chain: ControlFxChain) => ControlFxChain): ControlFxChain | null
    {
        const chain = this.snapshot.fxSlot === slot ? this.snapshot.fxChain : null;

        return chain === null ? null : change (chain);
    }

    private async runFx (slot: number,
                         request: () => Promise<ControlFxChain>,
                         optimistic: ControlFxChain | null): Promise<boolean>
    {
        const previous = this.snapshot.fxSlot === slot ? this.snapshot.fxChain : null;
        const sequence = this.fxSequence + 1;
        this.fxSequence = sequence;

        this.patch ({
            fxBusy: true,
            fxError: null,
            ...(optimistic !== null ? { fxChain: optimistic, fxSlot: slot } : {}),
        });

        try
        {
            const chain = await request ();

            if (sequence === this.fxSequence)
                this.patch ({ fxChain: chain, fxSlot: slot, fxBusy: false, fxError: null });

            return true;
        }
        catch (error)
        {
            if (sequence === this.fxSequence)
            {
                this.patch ({
                    fxBusy: false,
                    fxError: messageOf (error),
                    ...(previous !== null ? { fxChain: previous, fxSlot: slot } : {}),
                });
            }

            return false;
        }
    }

    /**
        Start, stop or reposition the transport.

        The remote applies the change optimistically so Play/Pause reacts on
        the next frame rather than after the HTTP round trip; a rejected
        request restores the previous transport.
    */
    updateTransport = async (patch: ControlTransportPatch): Promise<boolean> =>
    {
        const previous = this.snapshot.state;
        const sequence = this.transportSequence + 1;
        this.transportSequence = sequence;
        this.transportInFlight += 1;

        if (previous !== null)
            this.patch ({
                state: { ...previous, transport: { ...previous.transport, ...patch } },
                error: null,
            });

        try
        {
            const transport = await this.api.controlTransport (patch);

            if (sequence === this.transportSequence && this.snapshot.state !== null)
            {
                this.patch ({
                    state: { ...this.snapshot.state, transport },
                    error: null,
                });
            }

            return true;
        }
        catch (error)
        {
            if (sequence === this.transportSequence)
            {
                this.patch ({
                    error: messageOf (error),
                    ...(previous === null ? {} : { state: previous }),
                });
            }

            return false;
        }
        finally
        {
            this.transportInFlight -= 1;
        }
    };

    /**
        Panic: silence the rig by stopping the transport and disarming every
        slot. Both requests are attempted even if one fails, because a stuck
        note is worse than a stale status line.
    */
    panic = async (): Promise<boolean> =>
    {
        const disarmed = await this.armSlot (null);
        const stopped = await this.updateTransport ({
            playing: false,
            ppq: 0,
            positionSamples: 0,
        });

        return disarmed && stopped;
    };

    /**
        Apply an audio settings change (buffer size, sample rate, device or
        channel mapping).

        The response carries the post-change readout, so the panel updates
        from the same round trip that changed the engine.
    */
    updateAudio = async (patch: ControlAudioPatch): Promise<boolean> =>
    {
        this.patch ({ audioBusy: true, audioError: null });

        try
        {
            const audio = await this.api.updateAudio (patch);

            this.patch ({
                audio,
                audioBusy: false,
                audioError: null,
                measurement: null,
                measurementError: null,
            });

            return true;
        }
        catch (error)
        {
            this.patch ({ audioBusy: false, audioError: messageOf (error) });
            return false;
        }
    };

    /** Run the latency probe and show its result next to the readout. */
    measureLatency = async (): Promise<boolean> =>
    {
        this.patch ({ measurementError: null });

        try
        {
            const measurement = await this.api.measureLatency();

            this.patch ({ measurement, measurementError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ measurementError: messageOf (error) });
            return false;
        }
    };

    //==========================================================================
    // LAN pairing (T101)

    /**
        Turn LAN mode on/off.

        Enabling makes token auth mandatory. The desktop stays usable by
        pairing itself with the freshly issued code, so the user sees the QR
        and the live engine state at the same time.
    */
    toggleLan = async (enabled: boolean): Promise<boolean> =>
    {
        this.patch ({ pairingBusy: true, pairingError: null });

        try
        {
            let pairing = await this.api.setLanMode (enabled);
            let autoPairError: string | null = null;

            if (! enabled)
            {
                this.dropToken();
            }
            else if (this.api.token === null && isPairingCode (pairing.code))
            {
                // The listener rebinds to all interfaces right after the
                // toggle response. POST /pair may briefly see 503 or a closed
                // socket, so retry before giving up on the desktop's own token.
                for (let attempt = 0; attempt < 8 && this.api.token === null; attempt += 1)
                {
                    try
                    {
                        const result = await this.api.pair (pairing.code);
                        this.adoptToken (result.token);

                        try
                        {
                            pairing = await this.api.getPairing ();
                        }
                        catch
                        {
                            // The next poll picks the rotated code up.
                        }

                        autoPairError = null;
                        break;
                    }
                    catch (error)
                    {
                        autoPairError = messageOf (error);

                        const transient = ! (error instanceof ApiError)
                            || error.status === 503
                            || error.status === 0;

                        if (! transient)
                            break;

                        await new Promise (resolve => setTimeout (resolve, 120));
                    }
                }
            }

            this.patch ({
                pairing,
                pairingBusy: false,
                pairingError: autoPairError,
                unauthorized: false,
            });

            if (enabled)
                await this.refresh ();

            return true;
        }
        catch (error)
        {
            this.patch ({
                pairingBusy: false,
                pairingError: messageOf (error),
            });

            return false;
        }
    };

    /**
        Exchange a six-digit code for a token and keep it in this browser.

        Used by the tablet gate and by the manual fallback when the QR link is
        opened without a code.
    */
    pair = async (code: string): Promise<boolean> =>
    {
        this.patch ({ pairingBusy: true, pairingError: null });

        try
        {
            const result = await this.api.pair (code);
            this.adoptToken (result.token);

            let pairing = this.snapshot.pairing;

            try
            {
                pairing = await this.api.getPairing ();
            }
            catch
            {
                // The desktop-only endpoint is not readable from a tablet.
            }

            this.patch ({
                pairing,
                pairingBusy: false,
                pairingError: null,
                unauthorized: false,
                error: null,
            });

            await this.refresh ();
            return true;
        }
        catch (error)
        {
            this.patch ({ pairingBusy: false, pairingError: messageOf (error) });
            return false;
        }
    };

    /**
        Revoke every issued token, then restore this browser's own access so
        the desktop does not lock itself out of the engine it just secured.
    */
    forgetPairing = async (): Promise<boolean> =>
    {
        this.patch ({ pairingBusy: true, pairingError: null });

        try
        {
            let pairing = await this.api.revokePairing ();

            if (pairing.lanEnabled && isPairingCode (pairing.code))
            {
                const result = await this.api.pair (pairing.code);
                this.adoptToken (result.token);
                pairing = await this.api.getPairing ();
            }
            else
            {
                this.dropToken();
            }

            this.patch ({ pairing, pairingBusy: false, pairingError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ pairingBusy: false, pairingError: messageOf (error) });
            return false;
        }
    };

    private adoptToken (token: string): void
    {
        this.api.setToken (token);
        savePairingToken (token, this.options.storage);
    }

    private dropToken (): void
    {
        this.api.setToken (null);
        clearPairingToken (this.options.storage);
    }

    private get snapshot (): AppSnapshot
    {
        return this.core.getSnapshot ();
    }

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
        let merged = state;
        const current = this.snapshot.state;

        if (this.armInFlight > 0 && current !== null)
            merged = withArmedSlot (merged, current.armedSlot);

        if (this.transportInFlight > 0 && current !== null)
            merged = { ...merged, transport: current.transport };

        if (this.patchInFlight > 0 && this.pendingPatch !== null)
            merged = withPatchUpdate (merged, this.pendingPatch.slot, this.pendingPatch.patch);

        return merged;
    }

    private patch (change: Partial<AppSnapshot>): void
    {
        this.core.patch (change);
    }
}
