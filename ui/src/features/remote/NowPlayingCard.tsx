import type { ReactElement } from "react";

import { formatLatency, pad2 } from "./format";
import { currentPatch, type RemoteModel } from "./model";

export interface NowPlayingCardProps
{
    readonly model: RemoteModel;
    readonly loading: boolean;
    readonly onPrevious: (() => void) | undefined;
    readonly onNext: (() => void) | undefined;
}

export function NowPlayingCard ({
    model,
    loading,
    onPrevious,
    onNext,
}: NowPlayingCardProps): ReactElement
{
    const patch = currentPatch (model);
    const stepDisabled = model.patches.length === 0 || loading;

    return (
        <section
            className="remote__now"
            aria-label="Armed patch"
            aria-live="polite"
            data-testid="remote-current"
            data-slot={patch?.slot ?? -1}
        >
            <div className="remote__now-text">
                <p className="remote__label">Now playing</p>
                <h2 className="remote__now-name" data-testid="remote-current-name">
                    {loading
                        ? "Loading set…"
                        : patch === null
                            ? "Nothing armed"
                            : patch.name}
                </h2>
                <p className="remote__now-meta">
                    {patch === null
                        ? "Tap a patch or use Next to begin"
                        : `Patch ${pad2 (model.armedSlot + 1)} · chain ${formatLatency (patch.latencyMs)}`}
                </p>
            </div>

            <div className="remote__step">
                <button
                    className="remote__step-button"
                    type="button"
                    data-testid="remote-prev"
                    aria-label="Previous patch"
                    disabled={stepDisabled}
                    onClick={onPrevious}
                >
                    <span className="remote__step-arrow" aria-hidden="true">‹</span>
                    <span className="remote__step-label">Prev</span>
                </button>

                <button
                    className="remote__step-button"
                    type="button"
                    data-testid="remote-next"
                    aria-label="Next patch"
                    disabled={stepDisabled}
                    onClick={onNext}
                >
                    <span className="remote__step-label">Next</span>
                    <span className="remote__step-arrow" aria-hidden="true">›</span>
                </button>
            </div>
        </section>
    );
}
