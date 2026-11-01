import type { ControlPatch, ControlPlugin } from "../../api/client";

/**
    The slot the instrument page should show.

    The page is the *armed* instrument's page, so the controller wins whenever a
    patch is armed: a pad press or a grid click updates it on the next render.
    A `?slot=` link only chooses the patch while nothing is armed, which keeps a
    deep link useful without letting a stale URL override the controller.

    Other routes never resolve to an instrument slot. Arming therefore changes
    the instrument page in place and never navigates the user off Grid, Plugins
    or Audio.
*/
export function instrumentSlotForRoute (route: string,
                                        requestedSlot: number | null,
                                        armedSlot: number): number | null
{
    if (route !== "instrument")
        return null;

    if (armedSlot >= 0)
        return armedSlot;

    return requestedSlot;
}

/** True when the page is showing the controller's armed patch. */
export function followsArmedSlot (armedSlot: number): boolean
{
    return armedSlot >= 0;
}

/** Find one patch by its engine slot. */
export function patchForInstrumentSlot (patches: readonly ControlPatch[],
                                        slot: number | null): ControlPatch | null
{
    if (slot === null)
        return null;

    return patches.find (patch => patch.slot === slot) ?? null;
}

/** A plugin that can be assigned as a patch's instrument source. */
export function instrumentPlugins (plugins: readonly ControlPlugin[]): ControlPlugin[]
{
    return plugins.filter (plugin => plugin.isInstrument && ! plugin.isEffect);
}

/** A plugin that can be loaded as an insert. */
export function effectPlugins (plugins: readonly ControlPlugin[]): ControlPlugin[]
{
    return plugins.filter (plugin => plugin.isEffect);
}
