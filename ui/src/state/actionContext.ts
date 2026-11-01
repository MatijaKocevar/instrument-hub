import type { ControlApi } from "../api/client";
import type { AppSnapshot } from "./snapshot";

/**
    The store services a domain action module needs.

    The store owns the snapshot, the clock and the API façade; a domain module
    owns the requests and the optimistic bookkeeping for its API area. There is
    no domain knowledge here.
*/
export interface ActionContext
{
    readonly api: ControlApi;
    readonly now: () => number;
    readonly getSnapshot: () => AppSnapshot;
    readonly patch: (change: Partial<AppSnapshot>) => void;
}
