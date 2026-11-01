import type {
    ControlFxChain,
    ControlFxInsertRequest,
    ControlFxInsertUpdate,
} from "../api/client";
import {
    chainWithBypass,
    chainWithMove,
    chainWithoutInsert,
    moveOrder,
} from "../features/instrument/model";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Load and edit one patch's insert chain.

    Busy/error flags live on the snapshot so the Instrument page can render the
    request state, and bypass/remove/move update the visible chain before the
    engine answers. A rejection restores the last confirmed chain.
*/
export class FxActions
{
    /** Sequence of insert-chain requests; a stale response must not win. */
    private fxSequence = 0;

    constructor (private readonly context: ActionContext) {}

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
            const chain = await this.context.api.getFxChain (slot);

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
        this.runFx (slot, () => this.context.api.addFxInsert (slot, insert), null);

    /** Bypass, re-balance or re-parameterise one insert. */
    updateFxInsert = async (slot: number,
                            index: number,
                            update: ControlFxInsertUpdate): Promise<boolean> =>
        this.runFx (
            slot,
            () => this.context.api.updateFxInsert (slot, index, update),
            update.bypassed === undefined
                ? null
                : this.optimisticChain (slot, chain => chainWithBypass (chain, index, update.bypassed === true)));

    /** Remove one insert, dropping it from the page on the next render. */
    removeFxInsert = async (slot: number, index: number): Promise<boolean> =>
        this.runFx (
            slot,
            () => this.context.api.removeFxInsert (slot, index),
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
            () => this.context.api.reorderFxChain (slot, order),
            chainWithMove (chain, from, to));
    };

    /** Replace the chain with a played sequence, used by reload and tests. */
    replaceFxChain = async (slot: number,
                            inserts: readonly ControlFxInsertRequest[]): Promise<boolean> =>
        this.runFx (slot, () => this.context.api.replaceFxChain (slot, inserts), null);

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

    private get snapshot ()
    {
        return this.context.getSnapshot ();
    }

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
