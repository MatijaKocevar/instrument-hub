import type { ReactElement, ReactNode } from "react";

export interface RemoteLayoutProps
{
    readonly now: ReactNode;
    readonly setList: ReactNode;
    readonly transport: ReactNode;
    readonly panic: ReactNode;
}

/**
    The stage performance grid.

    The now-playing card spans the top, the set list takes the wide left
    column and the transport and panic panels stack on the right. Below
    820 px the CSS collapses the same four areas into one column.
*/
export function RemoteLayout ({
    now,
    setList,
    transport,
    panic,
}: RemoteLayoutProps): ReactElement
{
    return (
        <div className="remote__layout">
            {now}
            {setList}
            {transport}
            {panic}
        </div>
    );
}
