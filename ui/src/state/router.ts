import { useCallback, useEffect, useState } from "react";

/** Every page the shell can show. `/remote` is handled by the same bundle. */
export const SHELL_ROUTES = ["grid", "instrument", "plugins", "audio", "settings", "remote"] as const;

export type ShellRoute = (typeof SHELL_ROUTES)[number];

export interface ParsedRoute
{
    readonly route: ShellRoute;
    /** Path portion, always with a leading slash. */
    readonly pathname: string;
    /** Query string, including the leading `?`, or "". */
    readonly search: string;
}

export interface RouterState extends ParsedRoute
{
    /** Navigate inside the shell without reloading the document. */
    readonly navigate: (path: string) => void;
}

const DEFAULT_ROUTE: ShellRoute = "grid";

function isShellRoute (value: string): value is ShellRoute
{
    return (SHELL_ROUTES as readonly string[]).includes (value);
}

/** Split a path such as `/instrument?slot=3` into path and query. */
export function splitPath (path: string): { pathname: string; search: string }
{
    const queryAt = path.indexOf ("?");
    const rawPath = queryAt >= 0 ? path.slice (0, queryAt) : path;
    const search = queryAt >= 0 ? path.slice (queryAt) : "";
    const pathname = rawPath.length === 0 ? "/" : rawPath;

    return { pathname, search };
}

/**
    Resolve a URL pathname to a shell route.

    Unknown paths deliberately land on the grid: the engine's resource
    provider serves index.html for extensionless paths, so a typo must not
    leave the web view blank.
*/
export function routeFromPath (pathname: string): ShellRoute
{
    const segments = pathname.split ("/").filter (segment => segment.length > 0);
    const first = segments[0] ?? "";

    return isShellRoute (first) ? first : DEFAULT_ROUTE;
}

/** Canonical path for a route. */
export function pathForRoute (route: ShellRoute): string
{
    return `/${route}`;
}

/** Instrument page path for a patch, or the page without a selection. */
export function instrumentPathForSlot (slot: number | null): string
{
    if (slot === null || ! Number.isInteger (slot) || slot < 0)
        return "/instrument";

    return `/instrument?slot=${String (slot)}`;
}

/** Read the selected patch from an instrument-page query string. */
export function slotFromSearch (search: string): number | null
{
    const match = /(?:^\?|&)slot=(-?\d+)(?:&|$)/.exec (search);

    if (match === null || match[1] === undefined)
        return null;

    const slot = Number.parseInt (match[1], 10);

    return Number.isInteger (slot) && slot >= 0 ? slot : null;
}

/** Parse a complete path (`/audio`, `/instrument?slot=3`). */
export function parseRoute (path: string): ParsedRoute
{
    const { pathname, search } = splitPath (path);

    return { route: routeFromPath (pathname), pathname, search };
}

/** Current browser path, or the grid when rendering on the server. */
export function currentPath (): string
{
    if (typeof window === "undefined")
        return pathForRoute (DEFAULT_ROUTE);

    return `${window.location.pathname}${window.location.search}`;
}

/**
    Minimal history-based router.

    The app is embedded in a web view and served from a resource provider, so
    a dependency-free router that only pushes history entries is enough. Tests
    pass `initialPath` to render any page under `react-dom/server`.
*/
export function useRouter (initialPath?: string): RouterState
{
    const [path, setPath] = useState<string> (() => initialPath ?? currentPath ());

    useEffect (() =>
    {
        if (typeof window === "undefined")
            return;

        const onPopState = (): void => { setPath (currentPath ()); };

        window.addEventListener ("popstate", onPopState);

        return () => { window.removeEventListener ("popstate", onPopState); };
    }, []);

    const navigate = useCallback ((next: string): void =>
    {
        if (typeof window !== "undefined")
        {
            const current = `${window.location.pathname}${window.location.search}`;

            if (current !== next)
                window.history.pushState ({}, "", next);
        }

        setPath (next);
    }, []);

    const { pathname, search } = splitPath (path);

    return {
        route: routeFromPath (pathname),
        pathname,
        search,
        navigate,
    };
}
