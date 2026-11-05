import type { ReactElement } from "react";

import type { GridPageStatus } from "./model";

export interface RecoveringBannerProps {
  readonly page: GridPageStatus;
}

export function RecoveringBanner({
  page,
}: RecoveringBannerProps): ReactElement | null {
  if (page.status !== "recovering" && page.status !== "failed") return null;

  const failed = page.status === "failed";
  const detail = page.error.length > 0 ? ` ${page.error}` : "";

  return (
    <div
      className={`grid__page-status grid__page-status--${failed ? "failed" : "recovering"}`}
      role="status"
      data-testid="plugin-page-status"
      data-page-status={page.status}
    >
      <strong>
        {failed ? "Instruments unavailable." : "Recovering instruments…"}
      </strong>
      <span>
        {failed
          ? `${detail} Reload the page or reopen the project.`
          : " The shared plugin host is restarting and restoring every track."}
      </span>
    </div>
  );
}
