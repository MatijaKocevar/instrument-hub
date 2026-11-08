import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./trackGrid.css";

const TRACK_COLUMNS = 8;

function TrackColumnSkeleton({ index }: { index: number }): ReactElement {
  return (
    <section
      className="track-grid__column track-grid__column--skeleton"
      data-track-index={index}
      aria-hidden="true"
    >
      <div className="grid__cell track-grid__cell--track">
        <div className="track-grid__skeleton-track">
          <Skeleton width="30%" height="0.55rem" />
          <Skeleton width="82%" height="0.9rem" />
          <Skeleton width="46%" height="0.55rem" />
        </div>
      </div>

      <div className="grid__cell track-grid__cell--fx track-grid__skeleton-cell">
        <Skeleton width="58%" height="0.8rem" />
      </div>

      <div className="grid__cell track-grid__cell--fx track-grid__skeleton-cell">
        <Skeleton width="58%" height="0.8rem" />
      </div>

      <div className="grid__cell track-grid__cell--volume track-grid__skeleton-volume">
        <Skeleton width="68%" height="2.6rem" radius="999px" />
      </div>
    </section>
  );
}

export function TrackGridSkeleton(): ReactElement {
  return (
    <div className="grid__scroll" data-testid="grid-skeleton" aria-busy="true">
      <div
        className="track-grid__table track-grid__table--skeleton"
        aria-hidden="true"
      >
        {Array.from({ length: TRACK_COLUMNS }, (_, index) => (
          <TrackColumnSkeleton key={index} index={index} />
        ))}
      </div>
      <p className="loading-sr-only" role="status">
        Loading tracks…
      </p>
    </div>
  );
}
