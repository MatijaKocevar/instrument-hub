import { useEffect, useState, useSyncExternalStore } from "react";

import { MeterStore } from "../grid/meters";
import type { RemoteConnection } from "./model";

/**
    The remote's live-link view.

    `MeterStore` already owns the `/meters` socket and its reconnect loop; this
    keeps the extra history the stage UI needs: whether the link has ever come
    up, when it last heard from the engine, and how many times it dropped. That
    is what turns a bare `connected` flag into a reconnect banner.
*/
export interface RemoteLinkSnapshot
{
    /** True while the WebSocket is open. */
    readonly connected: boolean;
    /** True once the socket has opened at least once in this session. */
    readonly everConnected: boolean;
    /** `Date.now()` of the last open/frame, or null before the first. */
    readonly lastLiveAt: number | null;
    /** Disconnects since the last successful open. */
    readonly failures: number;
}

export const INITIAL_LINK: RemoteLinkSnapshot = {
    connected: false,
    everConnected: false,
    lastLiveAt: null,
    failures: 0,
};

export interface MeterLinkOptions
{
    /** Injectable clock, used by deterministic tests. */
    now?: () => number;
}

function sameLink (a: RemoteLinkSnapshot, b: RemoteLinkSnapshot): boolean
{
    return a.connected === b.connected
        && a.everConnected === b.everConnected
        && a.lastLiveAt === b.lastLiveAt
        && a.failures === b.failures;
}

/**
    Add link history on top of a `MeterStore` without duplicating its socket
    code. Subscribing before `start()` means the first open is never missed.

    The store's own reconnect policy applies unchanged (250 ms doubling,
    capped at 2 s for the stage), so a dropped socket is back well inside the
    remote's 3 s acceptance window.
*/
export class MeterLink
{
    private readonly meters: MeterStore;
    private readonly now: () => number;
    private readonly listeners = new Set<() => void> ();

    private snapshot: RemoteLinkSnapshot = INITIAL_LINK;
    private unsubscribe: (() => void) | null = null;
    private lastLiveAt: number | null = null;
    private wasConnected = false;

    constructor (meters: MeterStore, options: MeterLinkOptions = {})
    {
        this.meters = meters;
        this.now = options.now ?? (() => Date.now ());
    }

    getSnapshot = (): RemoteLinkSnapshot => this.snapshot;

    subscribe = (listener: () => void): (() => void) =>
    {
        this.listeners.add (listener);

        return () =>
        {
            this.listeners.delete (listener);
        };
    };

    /** Connect (and keep reconnecting) while the stage page is mounted. */
    start (): void
    {
        if (this.unsubscribe !== null)
            return;

        this.unsubscribe = this.meters.subscribe (this.sync);
        this.sync ();
        this.meters.start ();
    }

    /** Tear the socket down; the link returns to its initial state. */
    stop (): void
    {
        this.unsubscribe?. ();
        this.unsubscribe = null;

        this.meters.stop ();

        this.lastLiveAt = null;
        this.wasConnected = false;
        this.publish (INITIAL_LINK);
    }

    private sync = (): void =>
    {
        const meters = this.meters.getSnapshot ();

        if (meters.connected)
            this.lastLiveAt = this.now ();

        const failures = meters.connected
            ? 0
            : this.wasConnected
                ? this.snapshot.failures + 1
                : this.snapshot.failures;

        this.wasConnected = meters.connected;

        this.publish ({
            connected: meters.connected,
            everConnected: this.snapshot.everConnected || meters.connected,
            lastLiveAt: this.lastLiveAt,
            failures,
        });
    };

    private publish (next: RemoteLinkSnapshot): void
    {
        if (sameLink (this.snapshot, next))
            return;

        this.snapshot = next;

        for (const listener of this.listeners)
            listener ();
    }
}

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

/**
    Subscribe the stage to the live socket.

    The URL is stable for a session, so the store is created once. Server
    rendering never opens a socket: `start()` runs in an effect.
*/
export function useRemoteLink (url: string): RemoteLinkSnapshot
{
    const [link] = useState (() => new MeterLink (new MeterStore ({
        url,
        reconnectBaseMs: 250,
        reconnectMaxMs: 2000,
    })));

    useEffect (() =>
    {
        link.start ();

        return () =>
        {
            link.stop ();
        };
    }, [link]);

    return useSyncExternalStore (link.subscribe, link.getSnapshot, link.getSnapshot);
}
