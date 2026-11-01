import type { ReactElement } from "react";

import type { RemoteModel } from "./model";
import { SetListRow } from "./SetListRow";

export interface SetListProps
{
    readonly model: RemoteModel;
    readonly loading: boolean;
    readonly onSelect: ((slot: number) => void) | undefined;
}

/** The scrollable set list with one large target per patch. */
export function SetList ({ model, loading, onSelect }: SetListProps): ReactElement
{
    const empty = model.patches.length === 0;

    return (
        <section
            className="remote__set"
            aria-label="Set list"
            data-testid="remote-set"
        >
            <header className="remote__section-head">
                <h2 className="remote__section-title">Set list</h2>
                <span className="remote__count" data-testid="remote-set-count">
                    {model.patches.length} {model.patches.length === 1 ? "patch" : "patches"}
                </span>
            </header>

            {loading
                ? <div className="remote__set-loading" role="status" aria-busy="true">
                      Loading the set…
                  </div>
                : empty
                    ? <div className="remote__empty" data-testid="remote-empty">
                          No patches in this set yet.
                      </div>
                    : <ol className="remote__set-list" data-testid="remote-set-list">
                          {model.patches.map ((candidate, index) => (
                              <SetListRow
                                  key={candidate.id}
                                  patch={candidate}
                                  index={index}
                                  armed={candidate.slot === model.armedSlot}
                                  onSelect={onSelect}
                              />
                          ))}
                      </ol>}
        </section>
    );
}
