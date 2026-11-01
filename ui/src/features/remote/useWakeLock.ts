import { useEffect, useState } from "react";

import { WakeLockController } from "./WakeLockController";
import type { WakeLockCapableNavigator, WakeLockStatus } from "./wakeLockTypes";

/**
    Keep the screen awake for the lifetime of the calling component.

    Returns the current status so the remote can show a truthful "screen on"
    badge. Effects never run during server rendering, so the snapshot test
    never asks the host for a lock.
*/
export function useWakeLock (navigatorLike?: WakeLockCapableNavigator | null): WakeLockStatus
{
    const [status, setStatus] = useState<WakeLockStatus> ("idle");

    useEffect (() =>
    {
        const target = navigatorLike
            ?? (typeof navigator === "undefined"
                ? null
                : navigator as unknown as WakeLockCapableNavigator);

        const controller = new WakeLockController ({
            navigator: target,
            onStatus: setStatus,
        });

        void controller.start ();

        const onVisibilityChange = (): void =>
        {
            void controller.visibilityChanged (
                typeof document === "undefined" || document.visibilityState === "visible");
        };

        if (typeof document !== "undefined")
            document.addEventListener ("visibilitychange", onVisibilityChange);

        return () =>
        {
            if (typeof document !== "undefined")
                document.removeEventListener ("visibilitychange", onVisibilityChange);

            void controller.release ();
        };
    }, [navigatorLike]);

    return status;
}
