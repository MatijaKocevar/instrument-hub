/** Events the meter store subscribes to on its socket. */
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

/** Creates the socket the transport connects; tests inject a fake. */
export type MeterSocketFactory = (url: string) => MeterSocketLike;

export function defaultSocketFactory (url: string): MeterSocketLike
{
    const Constructor = (globalThis as {
        WebSocket?: new (url: string) => unknown;
    }).WebSocket;

    if (Constructor === undefined)
        throw new Error ("WebSocket is not available in this environment");

    return new Constructor (url) as MeterSocketLike;
}
