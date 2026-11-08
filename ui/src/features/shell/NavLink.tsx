import type { ReactElement } from "react";

import { pathForRoute, type ShellRoute } from "../../state/router";
import { RouteIcon } from "./RouteIcon";
import type { NavItem } from "./navigation";

export type NavLinkVariant = "sidebar" | "tab" | "footer";

export function NavLink({
  item,
  route,
  navigate,
  prefetch,
  variant = "sidebar",
}: {
  readonly item: NavItem;
  readonly route: ShellRoute;
  readonly navigate: (path: string) => void;
  readonly prefetch?: ((route: ShellRoute) => void) | undefined;
  readonly variant?: NavLinkVariant;
}): ReactElement {
  const active = route === item.route;
  const base = variant === "tab" ? "shell__tab" : "shell__link";
  const classes = [base];

  if (variant === "footer") classes.push("shell__link--footer");
  if (active) classes.push(`${base}--active`);

  return (
    <a
      className={classes.join(" ")}
      href={pathForRoute(item.route)}
      data-testid={`nav-${item.route}`}
      data-active={active ? "true" : "false"}
      data-workspace-item={
        variant === "footer" || item.route === "settings" ? "false" : "true"
      }
      aria-current={active ? "page" : undefined}
      title={variant === "tab" ? item.label : undefined}
      onPointerEnter={() => {
        prefetch?.(item.route);
      }}
      onFocus={() => {
        prefetch?.(item.route);
      }}
      onTouchStart={() => {
        prefetch?.(item.route);
      }}
      onClick={(event) => {
        event.preventDefault();
        navigate(pathForRoute(item.route));
      }}
    >
      <span className="shell__icon" aria-hidden="true">
        <RouteIcon route={item.route} />
      </span>
      <span className="shell__link-label">
        {variant === "tab" ? item.shortLabel : item.label}
      </span>
    </a>
  );
}
