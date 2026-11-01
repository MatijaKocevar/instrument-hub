import type { ReactElement, ReactNode } from "react";

export interface RemoteLayoutProps
{
    readonly now: ReactNode;
    readonly setList: ReactNode;
    readonly transport: ReactNode;
    readonly panic: ReactNode;
}

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
