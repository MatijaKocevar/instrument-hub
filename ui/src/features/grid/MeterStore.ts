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
    url: string;
    socketFactory?: MeterSocketFactory;
    reconnectBaseMs?: number;
    reconnectMaxMs?: number;
}

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

    start (): void
    {
        this.transport.start ();
    }

    stop (): void
    {
        this.transport.stop ();

        if (this.snapshot !== EMPTY_METERS)
            this.patch ({ connected: false, slots: {}, masterPeak: 0 });
    }

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
