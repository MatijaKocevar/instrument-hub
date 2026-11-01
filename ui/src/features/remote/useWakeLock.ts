import { useEffect, useState } from "react";

import { WakeLockController } from "./WakeLockController";
import type { WakeLockCapableNavigator, WakeLockStatus } from "./wakeLockTypes";

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
