import { useEffect, useState, useSyncExternalStore } from "react";

/** One slot's level in the latest meter frame. */
export interface MeterSlotReading
{
    readonly slot: number;
    readonly peak: number;
    readonly gain: number;
    readonly armed: boolean;
}

/** Immutable meter state, read through `useSyncExternalStore`. */
export interface MeterSnapshot
{
    /** True while the `/meters` socket is open. */
    readonly connected: boolean;
    /** Monotonic frame number from the server; gaps mean frames were skipped. */
    readonly sequence: number;
    readonly timestampMs: number;
    readonly masterPeak: number;
    /** Per-slot readings keyed by slot index. */
    readonly slots: Readonly<Record<number, MeterSlotReading>>;
}

export const EMPTY_METERS: MeterSnapshot = {
    connected: false,
    sequence: 0,
    timestampMs: 0,
    masterPeak: 0,
    slots: {},
};

/** Events the store subscribes to on its socket. */
export type MeterSocketEvent = "open" | "close" | "error" | "message";

export interface MeterSocketEventLike
{
    readonly data?: unknown;
}

/** The small slice of `WebSocket` the store needs; lets tests inject a fake. */
export interface MeterSocketLike
{
    addEventListener (type: MeterSocketEvent,
                      listener: (event: MeterSocketEventLike) => void): void;
    close (): void;
}

export interface MeterStoreOptions
{
    /** Absolute WebSocket URL, usually `ws://127.0.0.1:8080/meters`. */
    url: string;
    /** Injectable socket constructor, used by the headless tests. */
    socketFactory?: (url: string) => MeterSocketLike;
    /** First reconnect delay; doubles per failure. */
    reconnectBaseMs?: number;
    /** Upper bound for the reconnect delay. */
    reconnectMaxMs?: number;
}

interface ResolvedMeterOptions
{
    url: string;
    socketFactory: (url: string) => MeterSocketLike;
    reconnectBaseMs: number;
    reconnectMaxMs: number;
}

function defaultSocketFactory (url: string): MeterSocketLike
{
    const Constructor = (globalThis as {
        WebSocket?: new (url: string) => unknown;
    }).WebSocket;

    if (Constructor === undefined)
        throw new Error ("WebSocket is not available in this environment");

    return new Constructor (url) as MeterSocketLike;
}

function finiteOrZero (value: unknown): number
{
    return typeof value === "number" && Number.isFinite (value) ? value : 0;
}

interface ParsedFrame
{
    sequence: number;
    timestampMs: number;
    masterPeak: number;
    slots: MeterSlotReading[];
}

/**
    Validate a `/meters` frame.

    Unknown or malformed frames are ignored: a bad frame must never take the
    meters — or the grid — down. Frames may arrive as JSON text or already
    parsed objects.
*/
export function parseMeterFrame (value: unknown): ParsedFrame | null
{
    let candidate = value;

    if (typeof candidate === "string")
    {
        try
        {
            candidate = JSON.parse (candidate);
        }
        catch
        {
            return null;
        }
    }

    if (candidate === null || typeof candidate !== "object")
        return null;

    const frame = candidate as {
        type?: unknown;
        sequence?: unknown;
        timestampMs?: unknown;
        master?: { peak?: unknown };
        slots?: unknown;
    };

    if (frame.type !== "meters" || ! Array.isArray (frame.slots))
        return null;

    const slots: MeterSlotReading[] = [];

    for (const entry of frame.slots)
    {
        if (entry === null || typeof entry !== "object")
            continue;

        const reading = entry as {
            slot?: unknown;
            peak?: unknown;
            gain?: unknown;
            armed?: unknown;
        };

        if (typeof reading.slot !== "number" || ! Number.isInteger (reading.slot))
            continue;

        slots.push ({
            slot: reading.slot,
            peak: finiteOrZero (reading.peak),
            gain: finiteOrZero (reading.gain),
            armed: reading.armed === true,
        });
    }

    return {
        sequence: finiteOrZero (frame.sequence),
        timestampMs: finiteOrZero (frame.timestampMs),
        masterPeak: finiteOrZero (frame.master?.peak),
        slots,
    };
}

/**
    Latest meter frame from the WebSocket, with reconnect.

    One frame per broadcast tick (30 Hz by default). The store keeps only the
    newest frame; it never queues, which matches the server's contract that
    slow clients skip sequence numbers instead of receiving a backlog.
*/
export class MeterStore
{
    private readonly options: ResolvedMeterOptions;
    private readonly listeners = new Set<() => void>();

    private snapshot: MeterSnapshot = EMPTY_METERS;
    private socket: MeterSocketLike | null = null;
    private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
    private reconnectDelayMs: number;
    private running = false;

    constructor (options: MeterStoreOptions)
    {
        this.options = {
            url: options.url,
            socketFactory: options.socketFactory ?? defaultSocketFactory,
            reconnectBaseMs: options.reconnectBaseMs ?? 250,
            reconnectMaxMs: options.reconnectMaxMs ?? 5000,
        };

        this.reconnectDelayMs = this.options.reconnectBaseMs;
    }

    getSnapshot = (): MeterSnapshot => this.snapshot;

    subscribe = (listener: () => void): (() => void) =>
    {
        this.listeners.add (listener);

        return () =>
        {
            this.listeners.delete (listener);
        };
    };

    /** Connect and keep reconnecting. Safe to call more than once. */
    start (): void
    {
        if (this.running)
            return;

        this.running = true;
        this.connect ();
    }

    /** Close the socket, cancel a pending reconnect and mark meters idle. */
    stop (): void
    {
        this.running = false;

        if (this.reconnectTimer !== null)
        {
            clearTimeout (this.reconnectTimer);
            this.reconnectTimer = null;
        }

        const socket = this.socket;
        this.socket = null;
        socket?.close ();

        if (this.snapshot !== EMPTY_METERS)
            this.patch ({ connected: false, slots: {}, masterPeak: 0 });
    }

    /**
        Accept one frame. Returns false for a frame the UI cannot trust.

        Public so the headless test can drive the store without a browser.
    */
    ingest (value: unknown): boolean
    {
        const frame = parseMeterFrame (value);

        if (frame === null)
            return false;

        const slots: Record<number, MeterSlotReading> = {};

        for (const reading of frame.slots)
            slots[reading.slot] = reading;

        this.patch ({
            sequence: frame.sequence,
            timestampMs: frame.timestampMs,
            masterPeak: frame.masterPeak,
            slots,
        });

        return true;
    }

    private connect (): void
    {
        if (! this.running || this.options.url.length === 0)
            return;

        let socket: MeterSocketLike;

        try
        {
            socket = this.options.socketFactory (this.options.url);
        }
        catch
        {
            this.scheduleReconnect ();
            return;
        }

        this.socket = socket;

        socket.addEventListener ("open", () =>
        {
            if (this.socket !== socket)
                return;

            this.reconnectDelayMs = this.options.reconnectBaseMs;
            this.patch ({ connected: true });
        });

        socket.addEventListener ("message", event =>
        {
            if (this.socket !== socket)
                return;

            this.ingest (event.data);
        });

        const disconnect = (): void =>
        {
            if (this.socket !== socket)
                return;

            this.socket = null;
            this.patch ({ connected: false, slots: {}, masterPeak: 0 });
            this.scheduleReconnect ();
        };

        socket.addEventListener ("close", disconnect);
        socket.addEventListener ("error", disconnect);
    }

    private scheduleReconnect (): void
    {
        if (! this.running || this.reconnectTimer !== null)
            return;

        const delay = this.reconnectDelayMs;
        this.reconnectDelayMs = Math.min (this.options.reconnectMaxMs,
                                           this.reconnectDelayMs * 2);

        this.reconnectTimer = setTimeout (() =>
        {
            this.reconnectTimer = null;
            this.connect ();
        }, delay);
    }

    private patch (change: Partial<MeterSnapshot>): void
    {
        this.snapshot = { ...this.snapshot, ...change };

        for (const listener of this.listeners)
            listener ();
    }
}

/** Turn the control API origin into the `/meters` WebSocket URL. */
export function metersUrlFor (baseUrl: string): string
{
    try
    {
        const url = new URL (baseUrl);
        url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
        url.pathname = "/meters";
        url.search = "";
        url.hash = "";

        return url.toString ();
    }
    catch
    {
        return "";
    }
}

/**
    Subscribe a component to the meter stream.

    `useEffect` starts the socket, so server rendering renders zeroed meters
    and never opens a connection. The store is created once per component.
*/
export function useMeters (options: MeterStoreOptions): MeterSnapshot
{
    const [store] = useState (() => new MeterStore (options));

    useEffect (() =>
    {
        store.start ();

        return () =>
        {
            store.stop ();
        };
    }, [store]);

    return useSyncExternalStore (store.subscribe, store.getSnapshot, store.getSnapshot);
}
