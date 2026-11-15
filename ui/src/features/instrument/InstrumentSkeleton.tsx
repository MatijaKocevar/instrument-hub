import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./instrument.css";

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

      <div className="instrument__source-summary">
        <Skeleton width="55%" height="1.2rem" />
      </div>
      <div className="instrument__source-actions">
        <Skeleton width="100%" height="2.25rem" radius="8px" />
        <Skeleton width="100%" height="2.25rem" radius="8px" />
      </div>
      <Skeleton width="7rem" height="1.1rem" />
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
          <div className="instrument__insert-head">
            <Skeleton width="1.2rem" height="0.8rem" />
            <Skeleton width="5rem" height="0.8rem" />
            <Skeleton width="3.5rem" height="0.8rem" />
          </div>
          <Skeleton width="70%" height="0.9rem" />
          <div className="instrument__insert-meta">
            <Skeleton width="3.5rem" height="0.7rem" />
            <Skeleton width="3rem" height="0.7rem" />
          </div>
          <Skeleton width="100%" height="2rem" radius="6px" />
        </li>
      ))}
    </ol>
  );
}

function TrackStripSkeleton(): ReactElement {
  return (
    <aside
      className="instrument__strip"
      aria-hidden="true"
      data-testid="instrument-track-strip"
    >
      <div className="instrument__strip-head">
        <Skeleton width="5rem" height="0.7rem" />
        <Skeleton width="2rem" height="0.7rem" />
      </div>
      <div className="instrument__strip-body">
        <div className="track-grid__column instrument__live-track">
          <div className="track-grid__skeleton-track">
            <Skeleton width="4.5rem" height="0.9rem" />
            <Skeleton width="6rem" height="0.7rem" />
          </div>
          <div className="track-grid__skeleton-cell">
            <Skeleton width="100%" height="1.6rem" />
          </div>
          <div className="track-grid__skeleton-cell">
            <Skeleton width="100%" height="1.6rem" />
          </div>
          <div className="instrument__live-channel">
            <Skeleton width="4.5rem" height="0.7rem" />
            <Skeleton width="100%" height="0.4rem" radius="999px" />
            <Skeleton width="100%" height="1.6rem" radius="6px" />
          </div>
          <div className="track-grid__skeleton-volume">
            <Skeleton width="0.4rem" height="12rem" />
          </div>
        </div>
      </div>
    </aside>
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

      <div className="instrument__workspace">
        <div className="instrument__grid">
          <SourceSkeleton />

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
              <h3 className="instrument__card-title">Insert chain</h3>
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

            <div className="instrument__chain-row">
              <InsertRowsSkeleton />
              <div className="instrument__add" aria-hidden="true">
                <Skeleton width="5rem" height="0.8rem" />
                <Skeleton width="100%" height="2.5rem" radius="6px" />
                <Skeleton width="100%" height="2.75rem" radius="6px" />
              </div>
            </div>
          </section>
        </div>
        <TrackStripSkeleton />
      </div>

      <p className="loading-sr-only" role="status">
        Loading the instrument…
      </p>
    </section>
  );
}
