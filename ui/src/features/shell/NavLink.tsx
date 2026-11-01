import type { ReactElement } from "react";

import {
    pathForRoute,
    type ShellRoute,
} from "../../state/router";
import { RouteIcon } from "./RouteIcon";
import type { NavItem } from "./navigation";

/** One navigation destination, styled as a sidebar link or a bottom tab. */
export function NavLink ({ item, route, navigate, compact = false }: {
    readonly item: NavItem;
    readonly route: ShellRoute;
    readonly navigate: (path: string) => void;
    readonly compact?: boolean;
}): ReactElement
{
    const active = route === item.route;
    const className = compact
        ? `shell__tab${active ? " shell__tab--active" : ""}`
        : `shell__link${active ? " shell__link--active" : ""}`;

    return (
        <a
            className={className}
            href={pathForRoute (item.route)}
            data-testid={`nav-${item.route}`}
            data-active={active ? "true" : "false"}
            data-workspace-item={item.route === "settings" ? "false" : "true"}
            aria-current={active ? "page" : undefined}
            title={compact ? item.label : undefined}
            onClick={event =>
            {
                event.preventDefault ();
                navigate (pathForRoute (item.route));
            }}
        >
            <span className="shell__icon" aria-hidden="true">
                <RouteIcon route={item.route} />
            </span>
            <span className="shell__link-label">
                {compact ? item.shortLabel : item.label}
            </span>
        </a>
    );
}
