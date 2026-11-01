import type { RemoteLinkSnapshot } from "./MeterLink";
import type { RemoteConnection } from "./model";

export type RemoteBannerTone = "warning" | "critical";

export interface RemoteBanner
{
    readonly visible: boolean;
    readonly tone: RemoteBannerTone;
    readonly title: string;
    readonly detail: string;
}

const NO_BANNER: RemoteBanner = {
    visible: false,
    tone: "warning",
    title: "",
    detail: "",
};

export function remoteBanner (connection: RemoteConnection,
                              link: RemoteLinkSnapshot | null): RemoteBanner
{
    if (connection === "offline")
    {
        return {
            visible: true,
            tone: "critical",
            title: "Engine unreachable",
            detail: "Retrying automatically — controls return when the engine does.",
        };
    }

    if (link !== null && link.everConnected && !link.connected)
    {
        return {
            visible: true,
            tone: "warning",
            title: "Live link lost",
            detail: "Reconnecting automatically — the set list stays usable.",
        };
    }

    return NO_BANNER;
}
