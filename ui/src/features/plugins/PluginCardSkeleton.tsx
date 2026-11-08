import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./plugins.css";

export function PluginCardSkeleton(): ReactElement {
  return (
    <li className="plugins__card plugins__card--skeleton" aria-hidden="true">
      <header className="plugins__card-head">
        <div className="plugins__identity">
          <Skeleton width="62%" height="1rem" />
          <Skeleton width="38%" height="0.75rem" />
        </div>
        <Skeleton width="4rem" height="1.4rem" radius="999px" />
      </header>

      <Skeleton width="100%" height="0.75rem" />
      <Skeleton width="72%" height="0.75rem" />

      <div className="plugins__skeleton-meta">
        {[0, 1, 2].map((index) => (
          <div key={index}>
            <Skeleton width="55%" height="0.65rem" />
            <Skeleton width="80%" height="0.8rem" />
          </div>
        ))}
      </div>

      <div className="plugins__skeleton-path">
        <Skeleton width="28%" height="0.65rem" />
        <Skeleton width="100%" height="0.75rem" />
      </div>

      <Skeleton width="55%" height="1rem" />
    </li>
  );
}
