import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./instrument.css";

function MixerSkeleton(): ReactElement {
  return (
    <section
      className="instrument__card instrument__mix"
      aria-hidden="true"
      data-testid="instrument-mix"
    >
      <header className="instrument__card-head">
        <h3 className="instrument__card-title">Level</h3>
        <Skeleton width="7.5rem" height="0.7rem" />
      </header>

      <div className="instrument__skeleton-field">
        <Skeleton width="4rem" height="0.8rem" />
        <Skeleton height="0.6rem" radius="999px" />
        <Skeleton width="3rem" height="0.8rem" />
      </div>

      <Skeleton width="9rem" height="0.7rem" />

      <div className="instrument__skeleton-field">
        <Skeleton width="4rem" height="0.8rem" />
        <Skeleton height="0.6rem" radius="999px" />
        <Skeleton width="3rem" height="0.8rem" />
      </div>

      <div className="instrument__toggles">
        <Skeleton width="5rem" height="2.75rem" radius="10px" />
        <Skeleton width="5rem" height="2.75rem" radius="10px" />
      </div>
    </section>
  );
}

function SourceSkeleton(): ReactElement {
  return (
    <section
      className="instrument__card instrument__source-card"
      aria-hidden="true"
      data-testid="instrument-source-card"
    >
      <header className="instrument__card-head">
        <h3 className="instrument__card-title">Source</h3>
        <Skeleton width="4.5rem" height="0.7rem" />
      </header>

      <Skeleton width="55%" height="1.2rem" />
      <Skeleton width="32%" height="0.75rem" />
      <Skeleton width="100%" height="2.75rem" radius="10px" />
      <Skeleton width="100%" height="2.75rem" radius="10px" />
    </section>
  );
}

function InsertRowsSkeleton(): ReactElement {
  return (
    <ol className="instrument__inserts" aria-hidden="true">
      {[0, 1].map((index) => (
        <li
          className="instrument__insert instrument__insert--skeleton"
          key={index}
        >
          <Skeleton width="1.2rem" height="0.8rem" />
          <div className="instrument__skeleton-insert-name">
            <Skeleton width="65%" height="0.85rem" />
            <Skeleton width="45%" height="0.65rem" />
          </div>
          <Skeleton width="3.5rem" height="0.75rem" />
          <Skeleton width="3rem" height="0.75rem" />
          <Skeleton width="6.5rem" height="2rem" radius="10px" />
        </li>
      ))}
    </ol>
  );
}

export function InstrumentSkeleton({
  following,
}: {
  readonly following: boolean;
}): ReactElement {
  return (
    <section
      className="instrument"
      data-testid="instrument-page"
      data-slot={-1}
      data-following={following ? "true" : "false"}
      aria-busy="true"
    >
      <header className="instrument__head" aria-hidden="true">
        <Skeleton width="7.5rem" height="0.7rem" />
        <Skeleton width="min(24rem, 70%)" height="2.4rem" radius="0.5rem" />
        <Skeleton width="17rem" height="0.8rem" />
        <div className="instrument__badges">
          <Skeleton width="4.5rem" height="1.5rem" radius="999px" />
          <Skeleton width="6rem" height="1.5rem" radius="999px" />
        </div>
      </header>

      <div className="instrument__grid">
        <MixerSkeleton />
        <SourceSkeleton />
      </div>

      <section
        className="instrument__card instrument__chain-card"
        aria-label="Insert chain"
        aria-busy="true"
        data-testid="fx-chain"
        data-slot={-1}
        data-count={0}
        data-latency-samples={0}
        data-latency-ms="0.000"
        data-busy="true"
      >
        <header className="instrument__card-head">
          <h3 className="instrument__card-title">Inserts</h3>
          <Skeleton width="8rem" height="0.75rem" />
        </header>

        <dl
          className="instrument__meta instrument__meta--row"
          aria-hidden="true"
        >
          {[0, 1, 2].map((index) => (
            <div key={index}>
              <dt>Latency</dt>
              <dd>
                <Skeleton width="4rem" height="0.85rem" />
              </dd>
            </div>
          ))}
        </dl>

        <InsertRowsSkeleton />
        <Skeleton width="100%" height="2.75rem" radius="10px" />
      </section>

      <p className="loading-sr-only" role="status">
        Loading the instrument…
      </p>
    </section>
  );
}
