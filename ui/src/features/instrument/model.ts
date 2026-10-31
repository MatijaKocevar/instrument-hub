import type {
    ControlFxChain,
    ControlFxInsert,
    ControlFxInsertRequest,
    ControlPatch,
    ControlPatchSourceRequest,
    ControlPlugin,
} from "../../api/client";

/** Fader range, matching the API's `volumeDb` bounds. */
export const FADER_MIN_DB = -60;
export const FADER_MAX_DB = 12;
export const FADER_STEP_DB = 0.5;

/** The built-in effect types `POST /patches/{slot}/fx` accepts. */
export const BUILTIN_INSERT_TYPES = ["eq", "compressor", "delay", "reverb"] as const;

export type BuiltinInsertType = (typeof BUILTIN_INSERT_TYPES)[number];

/** Display names for the insert types. */
const INSERT_LABELS: Record<string, string> = {
    eq: "EQ",
    compressor: "Compressor",
    delay: "Delay",
    reverb: "Reverb",
    vst3: "VST3",
};

/** Human label for an insert type. */
export function insertLabel (type: string): string
{
    return INSERT_LABELS[type] ?? type;
}

/** Clamp a dB value into the fader's range. */
export function clampDb (db: number): number
{
    if (! Number.isFinite (db))
        return 0;

    return Math.min (FADER_MAX_DB, Math.max (FADER_MIN_DB, db));
}

/** Linear gain for a dB value; -60 dB is silence, exactly like the engine. */
export function gainFromDb (db: number): number
{
    const clamped = clampDb (db);

    return clamped <= FADER_MIN_DB ? 0 : 10 ** (clamped / 20);
}

/** Fixed-width dB readout; "-60.0 dB" is the engine's silence floor. */
export function formatDb (db: number): string
{
    return `${clampDb (db).toFixed (1)} dB`;
}

/** Clamp a balance value into -1 .. +1. */
export function clampPan (pan: number): number
{
    if (! Number.isFinite (pan))
        return 0;

    return Math.min (1, Math.max (-1, pan));
}

/** Balance readout: "L 40", "C", "R 40". */
export function formatPan (pan: number): string
{
    const clamped = clampPan (pan);

    if (Math.abs (clamped) < 0.005)
        return "C";

    return `${clamped < 0 ? "L" : "R"} ${String (Math.round (Math.abs (clamped) * 100))}`;
}

/** Wet amount as a whole percentage. */
export function wetPercent (wet: number): number
{
    return Math.round (Math.min (1, Math.max (0, Number.isFinite (wet) ? wet : 0)) * 100);
}

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

/** True when the patch's source is a hosted plugin (the editor has a window). */
export function isPluginSource (patch: ControlPatch | null): boolean
{
    return patch?.instrument.kind === "plugin";
}

/** Human label for the declared source kind. */
export function sourceKindLabel (kind: string): string
{
    switch (kind)
    {
        case "empty": return "Unassigned";
        case "plugin": return "VST3 plug-in";
        case "sampler": return "Sampler kit";
        case "audio-input": return "Live audio input";
        case "init-tone": return "Built-in tone";
        default: return "Unassigned";
    }
}

/** The source request that assigns an installed plugin to a patch. */
export function sourceRequestForPlugin (plugin: ControlPlugin): ControlPatchSourceRequest
{
    return {
        kind: "plugin",
        name: plugin.name,
        path: plugin.path,
        pluginFormat: plugin.format,
        pluginId: plugin.id,
        pluginName: plugin.name,
        pluginManufacturer: plugin.vendor,
        pluginVersion: plugin.version,
    };
}

/** The source request for the built-in initialisation tone. */
export function sourceRequestForInitTone (frequency?: number): ControlPatchSourceRequest
{
    return {
        kind: "init-tone",
        ...(frequency !== undefined ? { frequency } : {}),
    };
}

/** The source request that leaves a patch unassigned (silent). */
export function sourceRequestForEmpty (): ControlPatchSourceRequest
{
    return { kind: "empty" };
}

/** The insert request that loads an installed effect plugin. */
export function insertRequestForPlugin (plugin: ControlPlugin): ControlFxInsertRequest
{
    return {
        type: "vst3",
        path: plugin.path,
        id: plugin.id,
        name: plugin.name,
        bypassed: false,
        wet: 1,
        position: -1,
    };
}

/** The insert request for a built-in effect. */
export function insertRequestForBuiltin (type: BuiltinInsertType): ControlFxInsertRequest
{
    return { type, bypassed: false, wet: 1, position: -1 };
}

/** `order` that moves `from` to `to`, for `POST /patches/{slot}/fx/reorder`. */
export function moveOrder (length: number, from: number, to: number): number[]
{
    if (! Number.isInteger (length) || length <= 0)
        return [];

    const source = Math.min (length - 1, Math.max (0, from));
    const target = Math.min (length - 1, Math.max (0, to));

    const order = Array.from ({ length }, (_, index) => index);
    const [moved] = order.splice (source, 1);

    if (moved === undefined)
        return order;

    order.splice (target, 0, moved);

    return order;
}

/**
    Recompute a chain's latency budget after an optimistic edit.

    A bypassed insert (or one at `wet: 0`) contributes no latency, exactly like
    the engine. The millisecond conversion reuses the ratio in the chain the
    server last reported, so the readout stays consistent across a sample-rate
    change the UI has not seen.
*/
export function withChainLatency (chain: ControlFxChain,
                                  inserts: readonly ControlFxInsert[]): ControlFxChain
{
    const fxLatencySamples = inserts.reduce (
        (total, insert) => total + (insert.bypassed || insert.wet === 0 ? 0 : insert.latencySamples),
        0);

    const latencySamples = chain.instrumentLatencySamples + fxLatencySamples;
    const msPerSample = chain.latencySamples > 0
        ? chain.latencyMs / chain.latencySamples
        : 0;

    return {
        ...chain,
        fxLatencySamples,
        latencySamples,
        latencyMs: latencySamples * msPerSample,
        inserts: [...inserts].map ((insert, index) => ({ ...insert, index })),
    };
}

/** The chain as it will look after one insert is removed. */
export function chainWithoutInsert (chain: ControlFxChain, index: number): ControlFxChain
{
    if (index < 0 || index >= chain.inserts.length)
        return chain;

    return withChainLatency (chain, chain.inserts.filter ((_, position) => position !== index));
}

/** The chain as it will look after one insert is bypassed or re-enabled. */
export function chainWithBypass (chain: ControlFxChain, index: number, bypassed: boolean): ControlFxChain
{
    if (index < 0 || index >= chain.inserts.length)
        return chain;

    return withChainLatency (chain, chain.inserts.map ((insert, position) =>
        position === index ? { ...insert, bypassed } : insert));
}

/** The chain as it will look after one insert moves to `to`. */
export function chainWithMove (chain: ControlFxChain, from: number, to: number): ControlFxChain
{
    if (chain.inserts.length === 0)
        return chain;

    const order = moveOrder (chain.inserts.length, from, to);
    const reordered: ControlFxInsert[] = [];

    for (const index of order)
    {
        const insert = chain.inserts[index];

        if (insert !== undefined)
            reordered.push (insert);
    }

    return withChainLatency (chain, reordered);
}
