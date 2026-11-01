import type { ReactElement } from "react";

import type {
    ControlFxChain,
    ControlFxInsertRequest,
    ControlPatch,
    ControlPlugin,
} from "../../api/client";
import { InsertAdd } from "./InsertAdd";
import { InsertRow } from "./InsertRow";
import { latencyReadout } from "./latencyReadout";

interface InsertChainProps
{
    readonly patch: ControlPatch;
    readonly chain?: ControlFxChain | null;
    readonly chainError?: string | null;
    readonly chainBusy?: boolean;
    readonly plugins?: readonly ControlPlugin[] | null;
    readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
    readonly onRemoveInsert?: (slot: number, index: number) => void;
    readonly onToggleBypass?: (slot: number, index: number, bypassed: boolean) => void;
    readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
    readonly onRetryChain?: (slot: number) => void;
}

export function InsertChain ({
    patch,
    chain,
    chainError,
    chainBusy,
    plugins,
    onAddInsert,
    onRemoveInsert,
    onToggleBypass,
    onMoveInsert,
    onRetryChain,
}: InsertChainProps): ReactElement
{
    const inserts = chain?.inserts ?? [];

    return (
        <section className="instrument__card instrument__chain-card"
                 aria-label="Insert chain"
                 data-testid="fx-chain"
                 data-slot={patch.slot}
                 data-count={inserts.length}
                 data-latency-samples={chain?.latencySamples ?? 0}
                 data-latency-ms={(chain?.latencyMs ?? 0).toFixed (3)}
                 data-busy={chainBusy === true ? "true" : "false"}>
            <header className="instrument__card-head">
                <h3 className="instrument__card-title">Inserts</h3>
                <span className="instrument__card-hint" data-testid="fx-latency-total">
                    {chain === null || chain === undefined
                        ? "latency unavailable"
                        : latencyReadout (chain.latencySamples, chain.latencyMs)}
                </span>
            </header>

            <dl className="instrument__meta instrument__meta--row">
                <div>
                    <dt>Instrument</dt>
                    <dd data-testid="fx-latency-instrument">
                        {String (chain?.instrumentLatencySamples ?? 0)} smp
                    </dd>
                </div>
                <div>
                    <dt>Inserts</dt>
                    <dd data-testid="fx-latency-inserts">
                        {String (chain?.fxLatencySamples ?? 0)} smp
                    </dd>
                </div>
                <div>
                    <dt>Chain</dt>
                    <dd>{String (inserts.length)}</dd>
                </div>
            </dl>

            {chainError !== null && chainError !== undefined
                ? <div className="instrument__error" data-testid="fx-error" role="alert">
                      <span>{chainError}</span>
                      <button
                          className="instrument__button instrument__button--small"
                          type="button"
                          data-testid="fx-retry"
                          onClick={() => { onRetryChain?.(patch.slot); }}
                      >
                          Retry
                      </button>
                  </div>
                : null}

            {chain === null || chain === undefined
                ? <p className="instrument__empty" data-testid="fx-loading" aria-busy="true">
                      Loading the insert chain…
                  </p>
                : inserts.length === 0
                    ? <p className="instrument__empty" data-testid="fx-empty">
                          No inserts. The instrument goes straight to the bus.
                      </p>
                    : <ol className="instrument__inserts" data-testid="fx-inserts">
                          {inserts.map ((insert, index) => (
                              <InsertRow
                                  key={`${insert.id}-${String (index)}`}
                                  patch={patch}
                                  insert={insert}
                                  index={index}
                                  count={inserts.length}
                                  {...(onRemoveInsert !== undefined ? { onRemoveInsert } : {})}
                                  {...(onToggleBypass !== undefined ? { onToggleBypass } : {})}
                                  {...(onMoveInsert !== undefined ? { onMoveInsert } : {})}
                              />
                          ))}
                      </ol>}

            <InsertAdd
                patch={patch}
                {...(plugins !== undefined ? { plugins } : {})}
                {...(chainBusy !== undefined ? { chainBusy } : {})}
                {...(onAddInsert !== undefined ? { onAddInsert } : {})}
            />
        </section>
    );
}
