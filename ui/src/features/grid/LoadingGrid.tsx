import type { ReactElement } from "react";

import { GRID_COLUMNS, GRID_ROWS } from "./model";

/** The 8×8 skeleton shown while the first state arrives. */
export function LoadingGrid (): ReactElement
{
    return (
        <div className="grid__scroll">
            <div
                className="grid__loading"
                role="status"
                aria-busy="true"
                aria-label="Loading instruments"
                data-testid="grid-loading"
            >
                {Array.from ({ length: GRID_ROWS }, (_, row) => (
                    <div className="grid__loading-row" key={`loading-row-${String (row)}`}>
                        <span className="grid__loading-family" data-testid="grid-loading-family" />
                        {Array.from ({ length: GRID_COLUMNS }, (_, column) => (
                            <span
                                className="grid__loading-cell"
                                key={`loading-cell-${String (row)}-${String (column)}`}
                                data-testid="grid-skeleton-cell"
                            />
                        ))}
                    </div>
                ))}
            </div>
        </div>
    );
}
