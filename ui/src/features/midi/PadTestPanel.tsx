import type { ReactElement } from "react";

import type { ControlMidiState } from "../../api/client";
import { canRunPadTest, padTestLabel } from "./model";

interface PadTestPanelProps
{
    /** Latest `GET /midi`, used for availability and the result label. */
    readonly midi: ControlMidiState;
    /** True while the pad test is starting. */
    readonly busy: boolean;
    /** Message from a rejected pad test. */
    readonly error: string | null;
    /** Called by the pad-test button. */
    readonly onPadTest?: (() => void) | undefined;
}

/** The button that lights all 64 pads, its result and its availability hint. */
export function PadTestPanel ({
    midi,
    busy,
    error,
    onPadTest,
}: PadTestPanelProps): ReactElement
{
    return (
        <div className="midi__test" data-testid="pad-test-panel">
            <div className="midi__test-copy">
                <h3 className="midi__test-title">Pad test</h3>
                <p className="midi__test-lede">
                    Lights all 64 pads for about a second, then repaints the grid.
                </p>
            </div>

            <button
                className="midi__button"
                type="button"
                data-testid="pad-test"
                data-busy={busy ? "true" : "false"}
                data-active={midi.padTest.active ? "true" : "false"}
                disabled={busy || midi.padTest.active
                    || ! canRunPadTest (midi) || onPadTest === undefined}
                onClick={() => { onPadTest?.(); }}
            >
                {busy || midi.padTest.active ? "Lighting…" : "Light the grid"}
            </button>

            <span
                className={`midi__test-result${error !== null ? " midi__test-result--error" : ""}`}
                data-testid="pad-test-result"
                data-active={midi.padTest.active ? "true" : "false"}
                data-lit={midi.padTest.lit}
            >
                {padTestLabel (midi.padTest, error)}
            </span>

            {! canRunPadTest (midi) && error === null
                ? <span className="midi__test-hint" data-testid="pad-test-hint">
                      Connect the Launchpad and its MI port to run the test.
                  </span>
                : null}
        </div>
    );
}
