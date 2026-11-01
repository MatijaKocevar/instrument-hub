import type { ReactElement } from "react";

import type { PluginAssignment } from "./model";

interface PluginUsedByProps
{
    readonly usage: readonly PluginAssignment[];
}

function padSlot (slot: number): string
{
    return String (slot + 1).padStart (2, "0");
}

export function PluginUsedBy ({ usage }: PluginUsedByProps): ReactElement
{
    if (usage.length === 0)
    {
        return (
            <p className="plugins__unused" data-testid="plugin-unused">
                Not assigned to a patch.
            </p>
        );
    }

    return (
        <div
            className="plugins__used"
            data-testid="plugin-used-by"
            data-count={usage.length}
        >
            <span className="plugins__used-label">Used by</span>
            <ul className="plugins__used-list">
                {usage.map (assignment => (
                    <li
                        className="plugins__used-entry"
                        data-testid="plugin-used-by-entry"
                        data-slot={assignment.slot}
                        data-patch={assignment.patchName}
                        key={assignment.slot}
                    >
                        <span className="plugins__used-patch">
                            {assignment.patchName}
                        </span>
                        <span className="plugins__used-meta">
                            slot {padSlot (assignment.slot)} · {assignment.familyName}
                        </span>
                    </li>
                ))}
            </ul>
        </div>
    );
}
