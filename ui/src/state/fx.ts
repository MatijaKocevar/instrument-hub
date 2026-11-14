import type {
  ControlFxChain,
  ControlFxInsertRequest,
  ControlFxInsertUpdate,
  ControlFxTunerState,
} from "../api/client";
import {
  chainWithInsertUpdate,
  chainWithMove,
  chainWithoutInsert,
  moveOrder,
} from "../features/instrument";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class FxActions {
  private fxSequence = 0;

  constructor(private readonly context: ActionContext) {}

  loadFxChain = async (
    slot: number,
    options: { force?: boolean } = {},
  ): Promise<boolean> => {
    if (!Number.isInteger(slot) || slot < 0) return false;

    if (
      options.force !== true &&
      this.snapshot.fxSlot === slot &&
      this.snapshot.fxChain !== null
    )
      return true;

    this.patch({ fxBusy: true, fxError: null });

    try {
      const chain = await this.context.api.getFxChain(slot);

      this.patch({
        fxChain: chain,
        fxSlot: slot,
        fxBusy: false,
        fxError: null,
      });
      return true;
    } catch (error) {
      this.patch({
        fxBusy: false,
        fxError: messageOf(error),
        ...(this.snapshot.fxSlot === slot
          ? {}
          : { fxChain: null, fxSlot: slot }),
      });
      return false;
    }
  };

  getTuner = (
    slot: number,
    index: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxTunerState> =>
    this.context.api.getFxTuner(slot, index, options);

  addFxInsert = async (
    slot: number,
    insert: ControlFxInsertRequest,
  ): Promise<boolean> =>
    this.runFx(slot, () => this.context.api.addFxInsert(slot, insert), null);

  updateFxInsert = async (
    slot: number,
    index: number,
    update: ControlFxInsertUpdate,
  ): Promise<boolean> =>
    this.runFx(
      slot,
      () => this.context.api.updateFxInsert(slot, index, update),
      this.optimisticChain(slot, (chain) =>
        chainWithInsertUpdate(chain, index, update),
      ),
    );

  removeFxInsert = async (slot: number, index: number): Promise<boolean> =>
    this.runFx(
      slot,
      () => this.context.api.removeFxInsert(slot, index),
      this.optimisticChain(slot, (chain) => chainWithoutInsert(chain, index)),
    );

  moveFxInsert = async (
    slot: number,
    from: number,
    to: number,
  ): Promise<boolean> => {
    const chain = this.snapshot.fxSlot === slot ? this.snapshot.fxChain : null;

    if (chain === null) return false;

    const order = moveOrder(chain.inserts.length, from, to);

    return this.runFx(
      slot,
      () => this.context.api.reorderFxChain(slot, order),
      chainWithMove(chain, from, to),
    );
  };

  replaceFxChain = async (
    slot: number,
    inserts: readonly ControlFxInsertRequest[],
  ): Promise<boolean> =>
    this.runFx(
      slot,
      () => this.context.api.replaceFxChain(slot, inserts),
      null,
    );

  private optimisticChain(
    slot: number,
    change: (chain: ControlFxChain) => ControlFxChain,
  ): ControlFxChain | null {
    const chain = this.snapshot.fxSlot === slot ? this.snapshot.fxChain : null;

    return chain === null ? null : change(chain);
  }

  private async runFx(
    slot: number,
    request: () => Promise<ControlFxChain>,
    optimistic: ControlFxChain | null,
  ): Promise<boolean> {
    const previous =
      this.snapshot.fxSlot === slot ? this.snapshot.fxChain : null;
    const sequence = this.fxSequence + 1;
    this.fxSequence = sequence;

    this.patch({
      fxBusy: true,
      fxError: null,
      ...(optimistic !== null ? { fxChain: optimistic, fxSlot: slot } : {}),
    });

    try {
      const chain = await request();

      if (sequence === this.fxSequence)
        this.patch({
          fxChain: chain,
          fxSlot: slot,
          fxBusy: false,
          fxError: null,
        });

      return true;
    } catch (error) {
      if (sequence === this.fxSequence) {
        this.patch({
          fxBusy: false,
          fxError: messageOf(error),
          ...(previous !== null ? { fxChain: previous, fxSlot: slot } : {}),
        });
      }

      return false;
    }
  }

  private get snapshot() {
    return this.context.getSnapshot();
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
