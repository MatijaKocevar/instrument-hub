import type {
    MeterSocketEvent,
    MeterSocketEventLike,
    MeterSocketLike,
} from "../../features/grid";

export class FakeMeterSocket implements MeterSocketLike
{
    readonly listeners = new Map<MeterSocketEvent, ((event: MeterSocketEventLike) => void)[]> ();
    closed = false;

    addEventListener (type: MeterSocketEvent,
                      listener: (event: MeterSocketEventLike) => void): void
    {
        const existing = this.listeners.get (type) ?? [];
        existing.push (listener);
        this.listeners.set (type, existing);
    }

    close (): void
    {
        this.closed = true;
        this.emit ("close", {});
    }

    emit (type: MeterSocketEvent, event: MeterSocketEventLike): void
    {
        for (const listener of this.listeners.get (type) ?? [])
            listener (event);
    }
}
