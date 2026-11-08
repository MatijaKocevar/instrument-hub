import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import { PairingHeader } from "./PairingHeader";
import "./pairing.css";

export function PairingSkeleton({
  error = null,
}: {
  readonly error?: string | null;
}): ReactElement {
  return (
    <section
      className="pairing"
      aria-label="Tablet pairing"
      data-testid="pairing"
      aria-busy="true"
    >
      <PairingHeader lanEnabled={false} loading />

      {error !== null && error.length > 0 ? (
        <p className="pairing__error" role="alert" data-testid="pairing-error">
          {error}
        </p>
      ) : null}

      <div className="pairing__actions" aria-hidden="true">
        <Skeleton width="10rem" height="2.75rem" radius="0.75rem" />
        <Skeleton width="14rem" height="1rem" />
      </div>

      <div className="pairing__body" aria-hidden="true">
        <div className="pairing__qr-column">
          <Skeleton className="pairing__skeleton-qr" />
          <Skeleton width="9rem" height="0.8rem" />
        </div>

        <div className="pairing__details">
          <Skeleton width="7rem" height="0.75rem" />
          <Skeleton width="60%" height="3rem" radius="0.4rem" />

          <div className="pairing__skeleton-meta">
            {[0, 1, 2].map((index) => (
              <div key={index}>
                <Skeleton width="28%" height="0.8rem" />
                <Skeleton width="42%" height="0.8rem" />
              </div>
            ))}
          </div>

          <Skeleton width="10rem" height="2.75rem" radius="0.75rem" />
        </div>
      </div>

      <p
        className="loading-sr-only"
        role="status"
        data-testid="pairing-loading"
      >
        Reading the pairing state…
      </p>
    </section>
  );
}
