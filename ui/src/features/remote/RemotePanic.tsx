import type { ReactElement } from "react";

export interface RemotePanicProps
{
    readonly onPanic: (() => void) | undefined;
}

/** The oversized panic target that stops the transport and disarms the set. */
export function RemotePanic ({ onPanic }: RemotePanicProps): ReactElement
{
    return (
        <section
            className="remote__panic"
            aria-label="Panic"
            data-testid="remote-panic-panel"
        >
            <button
                className="remote__panic-button"
                type="button"
                data-testid="remote-panic"
                onClick={onPanic}
            >
                Panic
            </button>
            <p className="remote__panic-hint">
                Stops the transport and disarms every patch.
            </p>
        </section>
    );
}
