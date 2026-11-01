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
import { ApiError } from "../api/client";
import {
    clearPairingToken,
    isPairingCode,
    savePairingToken,
} from "../features/pairing/model";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";
import { FxActions } from "./fx";
import {
    resolveAppStoreOptions,
    type AppStoreOptions,
    type ResolvedAppStoreOptions,
} from "./options";
import { PatchArmActions } from "./patchArm";
import { PatchCaptureActions } from "./patchCapture";
import { PatchEditorActions } from "./patchEditor";
import { PatchSourceActions } from "./patchSource";
import { PatchUpdateActions } from "./patchUpdate";
import { PollLoop } from "./pollLoop";
import { refreshSnapshot } from "./refresh";
import type { AppSnapshot } from "./snapshot";
import { StoreCore } from "./storeCore";

export type { AppStoreOptions } from "./options";
export type { AppSnapshot, ConnectionPhase } from "./snapshot";

/**
    Holds the control state and keeps it fresh.

    A single polling loop serialises requests: `refresh()` de-duplicates while a
    request is in flight, so a slow engine can never stack requests. Failures
    back off exponentially (250 ms doubling to 5 s) and the last good state
    stays visible, which keeps the UI usable across an engine restart.

    Domain actions live in their own modules: patch arming, updates, source,
    editor and capture in `patch*.ts`, insert chains in `fx.ts`. This class
    keeps lifecycle, the public façade and the remaining domains until they
    move out too.

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

    /** Sequence of transport requests; a stale response must not win. */
    private transportSequence = 0;
    /** Number of transport requests in flight, used to keep Play/Pause during polls. */
    private transportInFlight = 0;

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

    /** Arm `slot` (or disarm with `null`), highlighting it before the request leaves. */
    armSlot = (slot: number | null): Promise<boolean> => this.patchArm.armSlot (slot);

    /** Change one patch's mix or colour, applying it optimistically. */
    updatePatch = (slot: number, patch: ControlPatchUpdate): Promise<boolean> =>
        this.patchUpdate.updatePatch (slot, patch);

    /** Recolour every patch in a family, one PATCH per patch. */
    updateFamilyColour = (familyId: string, colorIndex: number): Promise<boolean> =>
        this.patchUpdate.updateFamilyColour (familyId, colorIndex);

    /** Swap the patch's instrument source and re-read its chain latency. */
    changeInstrumentSource = (slot: number,
                              source: ControlPatchSourceRequest): Promise<boolean> =>
        this.patchSource.changeInstrumentSource (slot, source);

    /** Open the patch's native plugin editor. */
    openInstrumentEditor = (slot: number): Promise<boolean> =>
        this.patchEditor.openInstrumentEditor (slot);

    /** Close the patch's native editor. Idempotent. */
    closeInstrumentEditor = (slot: number): Promise<boolean> =>
        this.patchEditor.closeInstrumentEditor (slot);

    /** Save the running plugin's sound into its patch. */
    savePatchSound = (slot: number): Promise<boolean> =>
        this.patchCapture.savePatchSound (slot);

    //==========================================================================
    // Per-patch insert chain (T121)

    /** Load one patch's insert chain, reusing a chain already loaded for the slot. */
    loadFxChain = (slot: number, options: { force?: boolean } = {}): Promise<boolean> =>
        this.fx.loadFxChain (slot, options);

    /** Append an insert (built-in or VST3) to the patch's chain. */
    addFxInsert = (slot: number, insert: ControlFxInsertRequest): Promise<boolean> =>
        this.fx.addFxInsert (slot, insert);

    /** Bypass, re-balance or re-parameterise one insert. */
    updateFxInsert = (slot: number,
                      index: number,
                      update: ControlFxInsertUpdate): Promise<boolean> =>
        this.fx.updateFxInsert (slot, index, update);

    /** Remove one insert, dropping it from the page on the next render. */
    removeFxInsert = (slot: number, index: number): Promise<boolean> =>
        this.fx.removeFxInsert (slot, index);

    /** Move one insert `from` -> `to`; the engine answers with the new order. */
    moveFxInsert = (slot: number, from: number, to: number): Promise<boolean> =>
        this.fx.moveFxInsert (slot, from, to);

    /** Replace the chain with a played sequence, used by reload and tests. */
    replaceFxChain = (slot: number,
                      inserts: readonly ControlFxInsertRequest[]): Promise<boolean> =>
        this.fx.replaceFxChain (slot, inserts);

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
        let merged = this.patchArm.preservePending (state);

        merged = this.patchUpdate.preservePending (merged);

        const current = this.snapshot.state;

        if (this.transportInFlight > 0 && current !== null)
            merged = { ...merged, transport: current.transport };

        return merged;
    }

    private patch (change: Partial<AppSnapshot>): void
    {
        this.core.patch (change);
    }
}
