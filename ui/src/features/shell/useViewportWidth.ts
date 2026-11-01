import { useEffect, useState } from "react";

/** Width at or below which the sidebar collapses to bottom tabs. */
export const NARROW_BREAKPOINT = 900;

/**
    The viewport width is only needed to choose the shell variant. In the app
    the window is the source; the snapshot test supplies a width because
    `react-dom/server` has no viewport.
*/
export function useViewportWidth (initialWidth?: number | undefined): number
{
    const [width, setWidth] = useState<number> (
        () => initialWidth ?? (typeof window === "undefined" ? 1280 : window.innerWidth));

    useEffect (() =>
    {
        if (typeof window === "undefined")
            return undefined;

        const onResize = (): void => { setWidth (window.innerWidth); };

        window.addEventListener ("resize", onResize);

        return () => { window.removeEventListener ("resize", onResize); };
    }, []);

    return width;
}
