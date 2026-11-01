import type { ReactElement } from "react";

import type { RouterState } from "../../state/router";
import { BottomTabs } from "./BottomTabs";
import { RouteOutlet } from "./RouteOutlet";
import { ShellFooter } from "./ShellFooter";
import { Sidebar } from "./Sidebar";
import type { ShellNavProps } from "./navigation";
import { useShellData } from "./useShellData";
import {
    NARROW_BREAKPOINT,
    useViewportWidth,
} from "./useViewportWidth";

export interface AppShellProps
{
    readonly router: RouterState;
    readonly viewportWidth?: number | undefined;
}

export function AppShell ({ router, viewportWidth }: AppShellProps): ReactElement
{
    const shell = useShellData (router);
    const width = useViewportWidth (viewportWidth);
    const narrow = width < NARROW_BREAKPOINT;
    const { app, store, state } = shell;

    const navProps: ShellNavProps = {
        route: router.route,
        navigate: router.navigate,
        phase: app.phase,
        lanEnabled: app.pairing?.lanEnabled === true,
        paired: store.token !== null,
        version: shell.version,
    };

    return (
        <div
            className={`app-shell app-shell--${narrow ? "narrow" : "wide"}`}
            data-testid="app-shell"
            data-layout={narrow ? "narrow" : "wide"}
            data-viewport-width={width}
        >
            {narrow ? null : <Sidebar {...navProps} />}

            <div className="app-shell__main">
                <main className="app" data-route={router.route}>
                    {state === null && router.route !== "grid"
                        ? <section
                              className="app__empty"
                              aria-label="Engine state"
                              data-testid="state-placeholder"
                          >
                              Waiting for the engine to report its state…
                          </section>
                        : null}

                    <div className="app__route">
                        <RouteOutlet router={router} shell={shell} />
                    </div>

                    <ShellFooter shell={shell} />
                </main>
            </div>

            {narrow ? <BottomTabs {...navProps} /> : null}
        </div>
    );
}
