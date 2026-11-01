import type { ReactElement } from "react";

import type { ShellData } from "./useShellData";
import { PHASE_LABEL } from "./phaseLabel";

/** Footer line with the live connection phase and armed-slot summary. */
export function ShellFooter ({ shell }: { readonly shell: ShellData }): ReactElement
{
    const { app, state, armedSlot, version } = shell;

    return (
        <footer className="app__footer">
            <span
                className="app__status"
                data-testid="app-status"
                data-phase={app.phase}
            >
                {state === null
                    ? "Waiting for device state"
                    : `${PHASE_LABEL[app.phase]} · ${state.slots.length} slots · armed ${armedSlot < 0 ? "none" : armedSlot + 1}`}
            </span>
            <span className="app__version">v{version}</span>
        </footer>
    );
}
