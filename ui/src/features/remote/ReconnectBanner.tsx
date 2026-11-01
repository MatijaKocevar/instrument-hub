import type { ReactElement } from "react";

import { remoteBanner } from "./banner";
import { linkPhase, type RemoteLinkSnapshot } from "./MeterLink";
import type { RemoteConnection } from "./model";

export interface ReconnectBannerProps {
  readonly connection: RemoteConnection;
  readonly link: RemoteLinkSnapshot | null;
}

export function ReconnectBanner({
  connection,
  link,
}: ReconnectBannerProps): ReactElement | null {
  const banner = remoteBanner(connection, link);

  if (!banner.visible) return null;

  return (
    <div
      className={`remote__banner remote__banner--${banner.tone}`}
      role={banner.tone === "critical" ? "alert" : "status"}
      data-testid="remote-banner"
      data-tone={banner.tone}
      data-link={linkPhase(link)}
      data-failures={link?.failures ?? 0}
    >
      <span className="remote__banner-dot" aria-hidden="true" />
      <span className="remote__banner-text">
        <strong className="remote__banner-title">{banner.title}</strong>
        <span className="remote__banner-detail">{banner.detail}</span>
      </span>
    </div>
  );
}
