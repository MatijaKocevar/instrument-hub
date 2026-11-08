import type { ReactElement } from "react";

import type {
  ControlFxChain,
  ControlFxInsertRequest,
  ControlPatch,
  ControlPlugin,
  ControlTrack,
} from "../../api/client";
import { Skeleton } from "../loading";
import { InsertAdd } from "./InsertAdd";
import { InsertChainSkeleton } from "./InsertChainSkeleton";
import { InsertRow } from "./InsertRow";
import { latencyReadout } from "./latencyReadout";
import { hotFxName } from "./tracks";
import "./hotFx.css";

interface InsertChainProps {
  readonly patch: ControlPatch;
  readonly chain?: ControlFxChain | null;
  readonly chainError?: string | null;
  readonly chainBusy?: boolean;
  readonly plugins?: readonly ControlPlugin[] | null;
  readonly track?: ControlTrack | null;
  readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
  readonly onRemoveInsert?: (slot: number, index: number) => void;
  readonly onToggleBypass?: (
    slot: number,
    index: number,
    bypassed: boolean,
  ) => void;
  readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
  readonly onRetryChain?: (slot: number) => void;
  readonly onAssignHotFx?: (
    trackIndex: number,
    hot: number,
    insertId: string | null,
  ) => void;
}

export function InsertChain({
  patch,
  chain,
  chainError,
  chainBusy,
  plugins,
  track = null,
  onAddInsert,
  onRemoveInsert,
  onToggleBypass,
  onMoveInsert,
  onRetryChain,
  onAssignHotFx,
}: InsertChainProps): ReactElement {
  const inserts = chain?.inserts ?? [];
  const missing = chain === null || chain === undefined;
  const showSkeleton =
    missing && (chainError === null || chainError === undefined);

  return (
    <section
      className="instrument__card instrument__chain-card"
      aria-label="Insert chain"
      aria-busy={chainBusy === true}
      data-testid="fx-chain"
      data-slot={patch.slot}
      data-count={inserts.length}
      data-latency-samples={chain?.latencySamples ?? 0}
      data-latency-ms={(chain?.latencyMs ?? 0).toFixed(3)}
      data-busy={chainBusy === true ? "true" : "false"}
    >
      <header className="instrument__card-head">
        <h3 className="instrument__card-title">Inserts</h3>
        <span className="instrument__card-hint" data-testid="fx-latency-total">
          {missing ? (
            showSkeleton ? (
              <Skeleton width="8rem" height="0.75rem" />
            ) : (
              "latency unavailable"
            )
          ) : (
            latencyReadout(chain.latencySamples, chain.latencyMs)
          )}
        </span>
      </header>

      {track !== null ? (
        <p
          className="instrument__hot-summary"
          data-testid="track-hot-summary"
          data-track-index={track.index}
          data-hot-1={hotFxName(track, 0)}
          data-hot-2={hotFxName(track, 1)}
        >
          <span>Hot FX 1 · {hotFxName(track, 0)}</span>
          <span>Hot FX 2 · {hotFxName(track, 1)}</span>
        </p>
      ) : null}

      <dl className="instrument__meta instrument__meta--row">
        <div>
          <dt>Instrument</dt>
          <dd data-testid="fx-latency-instrument">
            {missing ? (
              showSkeleton ? (
                <Skeleton width="4rem" height="0.85rem" />
              ) : (
                "0 smp"
              )
            ) : (
              `${String(chain.instrumentLatencySamples)} smp`
            )}
          </dd>
        </div>
        <div>
          <dt>Inserts</dt>
          <dd data-testid="fx-latency-inserts">
            {missing ? (
              showSkeleton ? (
                <Skeleton width="4rem" height="0.85rem" />
              ) : (
                "0 smp"
              )
            ) : (
              `${String(chain.fxLatencySamples)} smp`
            )}
          </dd>
        </div>
        <div>
          <dt>Chain</dt>
          <dd>
            {missing ? (
              showSkeleton ? (
                <Skeleton width="2rem" height="0.85rem" />
              ) : (
                String(inserts.length)
              )
            ) : (
              String(inserts.length)
            )}
          </dd>
        </div>
      </dl>

      {chainError !== null && chainError !== undefined ? (
        <div className="instrument__error" data-testid="fx-error" role="alert">
          <span>{chainError}</span>
          <button
            className="instrument__button instrument__button--small"
            type="button"
            data-testid="fx-retry"
            onClick={() => {
              onRetryChain?.(patch.slot);
            }}
          >
            Retry
          </button>
        </div>
      ) : null}

      {missing ? (
        showSkeleton ? (
          <InsertChainSkeleton />
        ) : null
      ) : inserts.length === 0 ? (
        <p className="instrument__empty" data-testid="fx-empty">
          No inserts. The instrument goes straight to the bus.
        </p>
      ) : (
        <ol className="instrument__inserts" data-testid="fx-inserts">
          {inserts.map((insert, index) => (
            <InsertRow
              key={`${insert.id}-${String(index)}`}
              patch={patch}
              insert={insert}
              index={index}
              count={inserts.length}
              track={track}
              {...(onRemoveInsert !== undefined ? { onRemoveInsert } : {})}
              {...(onToggleBypass !== undefined ? { onToggleBypass } : {})}
              {...(onMoveInsert !== undefined ? { onMoveInsert } : {})}
              {...(onAssignHotFx !== undefined ? { onAssignHotFx } : {})}
            />
          ))}
        </ol>
      )}

      <InsertAdd
        patch={patch}
        {...(plugins !== undefined ? { plugins } : {})}
        {...(chainBusy !== undefined ? { chainBusy } : {})}
        {...(onAddInsert !== undefined ? { onAddInsert } : {})}
      />
    </section>
  );
}
