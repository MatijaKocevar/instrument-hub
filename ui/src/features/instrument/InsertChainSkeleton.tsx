import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./instrument.css";

export function InsertChainSkeleton(): ReactElement {
  return (
    <>
      <ol
        className="instrument__inserts"
        data-testid="fx-loading"
        aria-busy="true"
      >
        {[0, 1].map((index) => (
          <li
            className="instrument__insert instrument__insert--skeleton"
            key={index}
            aria-hidden="true"
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
      <p className="loading-sr-only" role="status">
        Loading the insert chain…
      </p>
    </>
  );
}
