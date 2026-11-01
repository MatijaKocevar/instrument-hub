import type { RemoteLinkSnapshot } from "./MeterLink";
import type { RemoteConnection } from "./model";

/** What the banner should say, if anything. */
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

/**
    Decide whether the stage needs a reconnect banner.

    Two independent links can fail: the HTTP poll behind the set list
    (`connection`) and the live WebSocket. A dead API is critical because new
    taps cannot land; a dead socket is a warning because the set list and the
    optimistic controls still work and the socket retries on its own.
*/
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
