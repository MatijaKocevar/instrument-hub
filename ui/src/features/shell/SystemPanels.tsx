import type { ReactElement } from "react";

import type { ControlState } from "../../api/client";
import { EnginePanel } from "./EnginePanel";
import { SettingsPanel } from "./SettingsPanel";
import { TransportPanel } from "./TransportPanel";

export function SystemPanels ({ state }: { readonly state: ControlState }): ReactElement
{
    return (
        <section className="app__panels" aria-label="Engine state">
            <EnginePanel state={state} />
            <TransportPanel transport={state.transport} />
            <SettingsPanel settings={state.settings} />
        </section>
    );
}
