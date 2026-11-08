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
      <p className="loading-sr-only" role="status">
        Loading the insert chain…
      </p>
    </>
  );
}
