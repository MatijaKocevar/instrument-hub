import { useEffect, useState, useSyncExternalStore } from "react";

import { MeterStore, type MeterStoreOptions } from "./MeterStore";
import type { MeterSnapshot } from "./meterFrame";

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
