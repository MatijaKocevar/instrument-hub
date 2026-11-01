import {
    EMPTY_METERS,
    parseMeterFrame,
    type MeterSnapshot,
    type MeterSlotReading,
} from "./meterFrame";
import { defaultSocketFactory, type MeterSocketFactory } from "./meterSocket";
import { MeterTransport } from "./meterTransport";

export interface MeterStoreOptions
{
    /** Absolute WebSocket URL, usually `ws://127.0.0.1:8080/meters`. */
    url: string;
    /** Injectable socket constructor, used by the headless tests. */
    socketFactory?: MeterSocketFactory;
    /** First reconnect delay; doubles per failure. */
    reconnectBaseMs?: number;
    /** Upper bound for the reconnect delay. */
    reconnectMaxMs?: number;
}

/**
    Latest meter frame from the WebSocket, with reconnect.

    One frame per broadcast tick (30 Hz by default). The store keeps only the
    newest frame; it never queues, which matches the server's contract that
    slow clients skip sequence numbers instead of receiving a backlog.
*/
export class MeterStore
{
    private readonly listeners = new Set<() => void> ();
    private readonly transport: MeterTransport;

    private snapshot: MeterSnapshot = EMPTY_METERS;

    constructor (options: MeterStoreOptions)
    {
        this.transport = new MeterTransport ({
            url: options.url,
            socketFactory: options.socketFactory ?? defaultSocketFactory,
            reconnectBaseMs: options.reconnectBaseMs ?? 250,
            reconnectMaxMs: options.reconnectMaxMs ?? 5000,
            onOpen: () => { this.patch ({ connected: true }); },
            onMessage: data => { this.ingest (data); },
            onDisconnect: () => {
                this.patch ({ connected: false, slots: {}, masterPeak: 0 });
            },
        });
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
        this.transport.start ();
    }

    /** Close the socket, cancel a pending reconnect and mark meters idle. */
    stop (): void
    {
        this.transport.stop ();

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

    private patch (change: Partial<MeterSnapshot>): void
    {
        this.snapshot = { ...this.snapshot, ...change };

        for (const listener of this.listeners)
            listener ();
    }
}
