import { useCallback, useEffect, useState } from "react";

export const SHELL_ROUTES = ["grid", "instrument", "plugins", "audio", "settings", "remote"] as const;

export type ShellRoute = (typeof SHELL_ROUTES)[number];

export interface ParsedRoute
{
    readonly route: ShellRoute;
    readonly pathname: string;
    readonly search: string;
}

export interface RouterState extends ParsedRoute
{
    readonly navigate: (path: string) => void;
}

const DEFAULT_ROUTE: ShellRoute = "grid";

function isShellRoute (value: string): value is ShellRoute
{
    return (SHELL_ROUTES as readonly string[]).includes (value);
}

export function splitPath (path: string): { pathname: string; search: string }
{
    const queryAt = path.indexOf ("?");
    const rawPath = queryAt >= 0 ? path.slice (0, queryAt) : path;
    const search = queryAt >= 0 ? path.slice (queryAt) : "";
    const pathname = rawPath.length === 0 ? "/" : rawPath;

    return { pathname, search };
}

export function routeFromPath (pathname: string): ShellRoute
{
    const segments = pathname.split ("/").filter (segment => segment.length > 0);
    const first = segments[0] ?? "";

    return isShellRoute (first) ? first : DEFAULT_ROUTE;
}

export function pathForRoute (route: ShellRoute): string
{
    return `/${route}`;
}

export function instrumentPathForSlot (slot: number | null): string
{
    if (slot === null || ! Number.isInteger (slot) || slot < 0)
        return "/instrument";

    return `/instrument?slot=${String (slot)}`;
}

export function slotFromSearch (search: string): number | null
{
    const match = /(?:^\?|&)slot=(-?\d+)(?:&|$)/.exec (search);

    if (match === null || match[1] === undefined)
        return null;

    const slot = Number.parseInt (match[1], 10);

    return Number.isInteger (slot) && slot >= 0 ? slot : null;
}

export function parseRoute (path: string): ParsedRoute
{
    const { pathname, search } = splitPath (path);

    return { route: routeFromPath (pathname), pathname, search };
}

export function currentPath (): string
{
    if (typeof window === "undefined")
        return pathForRoute (DEFAULT_ROUTE);

    return `${window.location.pathname}${window.location.search}`;
}

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
