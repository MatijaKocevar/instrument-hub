import { useState, type ReactElement } from "react";

import type {
    ControlFxChain,
    ControlFxInsertRequest,
    ControlPatch,
    ControlPatchSourceRequest,
    ControlPatchStateCapture,
    ControlPlugin,
} from "../../api/client";
import { colourForIndex, nextColourIndex } from "../grid";
import {
    BUILTIN_INSERT_TYPES,
    FADER_MAX_DB,
    FADER_MIN_DB,
    FADER_STEP_DB,
    clampDb,
    clampPan,
    effectPlugins,
    formatDb,
    formatPan,
    formatStateSize,
    gainFromDb,
    instrumentPlugins,
    insertLabel,
    insertRequestForBuiltin,
    insertRequestForPlugin,
    isPluginSource,
    sourceKindLabel,
    sourceRequestForEmpty,
    sourceRequestForInitTone,
    sourceRequestForPlugin,
    wetPercent,
    type BuiltinInsertType,
} from "./model";
import "./instrument.css";

export interface InstrumentPageProps
{
    /** The patch to show, or null when nothing is selected. */
    readonly patch: ControlPatch | null;
    /** Slot the engine currently drives from the controller. */
    readonly armedSlot: number;
    /** True when the page follows the controller instead of a pinned `?slot=`. */
    readonly following?: boolean;
    /** Ordered inserts and latency budget, or null while loading. */
    readonly chain?: ControlFxChain | null;
    readonly chainError?: string | null;
    /** True while an insert request is in flight. */
    readonly chainBusy?: boolean;
    /** True while a source change is in flight. */
    readonly sourceBusy?: boolean;
    /** True while the patch's native editor window is open. */
    readonly editorOpen?: boolean;
    /** True while an editor open/close request is in flight. */
    readonly editorBusy?: boolean;
    /** Message from the most recent editor failure. */
    readonly editorError?: string | null;
    /** True while an explicit save of the patch's sound is in flight. */
    readonly captureBusy?: boolean;
    /** Confirmation of the most recent explicit save for this patch. */
    readonly capture?: ControlPatchStateCapture | null;
    /** Message from the most recent failed explicit save. */
    readonly captureError?: string | null;
    /** Installed plugins used by the source and insert pickers. */
    readonly plugins?: readonly ControlPlugin[] | null;
    readonly pluginError?: string | null;
    readonly loading?: boolean;

    readonly onVolumeChange?: (slot: number, volumeDb: number) => void;
    readonly onPanChange?: (slot: number, pan: number) => void;
    readonly onMuteChange?: (slot: number, muted: boolean) => void;
    readonly onSoloChange?: (slot: number, solo: boolean) => void;
    readonly onColourChange?: (slot: number, colorIndex: number) => void;
    readonly onSourceChange?: (slot: number, source: ControlPatchSourceRequest) => void;
    readonly onOpenEditor?: (slot: number) => void;
    readonly onCloseEditor?: (slot: number) => void;
    readonly onCaptureState?: (slot: number) => void;
    readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
    readonly onRemoveInsert?: (slot: number, index: number) => void;
    readonly onToggleBypass?: (slot: number, index: number, bypassed: boolean) => void;
    readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
    readonly onRetryChain?: (slot: number) => void;
    readonly onBack?: () => void;
}

function latencyReadout (samples: number, milliseconds: number): string
{
    return `${Math.max (0, milliseconds).toFixed (2)} ms · ${String (Math.max (0, samples))} smp`;
}

function MixSection ({ patch, onVolumeChange, onPanChange, onMuteChange, onSoloChange }: {
    readonly patch: ControlPatch;
    readonly onVolumeChange?: (slot: number, volumeDb: number) => void;
    readonly onPanChange?: (slot: number, pan: number) => void;
    readonly onMuteChange?: (slot: number, muted: boolean) => void;
    readonly onSoloChange?: (slot: number, solo: boolean) => void;
}): ReactElement
{
    const volumeDb = clampDb (patch.volumeDb);
    const pan = clampPan (patch.pan);

    return (
        <section className="instrument__card instrument__mix"
                 aria-label="Patch mixer"
                 data-testid="instrument-mix"
                 data-volume-db={volumeDb.toFixed (2)}
                 data-gain={gainFromDb (volumeDb).toFixed (6)}
                 data-pan={pan.toFixed (2)}>
            <header className="instrument__card-head">
                <h3 className="instrument__card-title">Level</h3>
                <span className="instrument__card-hint">Audible on the next block</span>
            </header>

            <label className="instrument__field" htmlFor="instrument-volume">
                <span className="instrument__field-label">Volume</span>
                <input
                    id="instrument-volume"
                    className="instrument__fader"
                    type="range"
                    min={FADER_MIN_DB}
                    max={FADER_MAX_DB}
                    step={FADER_STEP_DB}
                    value={volumeDb}
                    data-testid="volume-fader"
                    data-slot={patch.slot}
                    data-db={volumeDb.toFixed (2)}
                    aria-valuetext={formatDb (volumeDb)}
                    onChange={event => {
                        onVolumeChange?.(patch.slot, Number (event.currentTarget.value));
                    }}
                />
                <output className="instrument__value" data-testid="volume-value">
                    {formatDb (volumeDb)}
                </output>
            </label>

            <p className="instrument__field-hint" data-testid="volume-gain"
               data-gain={gainFromDb (volumeDb).toFixed (6)}>
                effective gain ×{gainFromDb (volumeDb).toFixed (3)}
            </p>

            <label className="instrument__field" htmlFor="instrument-pan">
                <span className="instrument__field-label">Balance</span>
                <input
                    id="instrument-pan"
                    className="instrument__fader instrument__fader--pan"
                    type="range"
                    min={-1}
                    max={1}
                    step={0.01}
                    value={pan}
                    data-testid="pan-slider"
                    data-slot={patch.slot}
                    data-pan={pan.toFixed (2)}
                    aria-valuetext={formatPan (pan)}
                    onChange={event => {
                        onPanChange?.(patch.slot, Number (event.currentTarget.value));
                    }}
                />
                <output className="instrument__value" data-testid="pan-value">
                    {formatPan (pan)}
                </output>
            </label>

            <div className="instrument__toggles" role="group" aria-label="Mute and solo">
                <button
                    className={`instrument__toggle${patch.mute ? " instrument__toggle--on" : ""}`}
                    type="button"
                    aria-pressed={patch.mute}
                    data-testid="mute-button"
                    data-slot={patch.slot}
                    data-muted={patch.mute ? "true" : "false"}
                    onClick={() => { onMuteChange?.(patch.slot, ! patch.mute); }}
                >
                    Mute
                </button>
                <button
                    className={`instrument__toggle${patch.solo ? " instrument__toggle--on" : ""}`}
                    type="button"
                    aria-pressed={patch.solo}
                    data-testid="solo-button"
                    data-slot={patch.slot}
                    data-solo={patch.solo ? "true" : "false"}
                    onClick={() => { onSoloChange?.(patch.slot, ! patch.solo); }}
                >
                    Solo
                </button>
            </div>
        </section>
    );
}

function SourceSection ({ patch, plugins, pluginError, sourceBusy, editorOpen, editorBusy, editorError, captureBusy, capture, captureError, onSourceChange, onOpenEditor, onCloseEditor, onCaptureState }: {
    readonly patch: ControlPatch;
    readonly plugins?: readonly ControlPlugin[] | null;
    readonly pluginError?: string | null;
    readonly sourceBusy?: boolean;
    readonly editorOpen?: boolean;
    readonly editorBusy?: boolean;
    readonly editorError?: string | null;
    readonly captureBusy?: boolean;
    readonly capture?: ControlPatchStateCapture | null;
    readonly captureError?: string | null;
    readonly onSourceChange?: (slot: number, source: ControlPatchSourceRequest) => void;
    readonly onOpenEditor?: (slot: number) => void;
    readonly onCloseEditor?: (slot: number) => void;
    readonly onCaptureState?: (slot: number) => void;
}): ReactElement
{
    const instrument = patch.instrument;
    const [selection, setSelection] = useState("");
    const assignable = instrumentPlugins (plugins ?? []);
    const isPlugin = isPluginSource (patch);
    const editor = editorOpen === true || patch.editorOpen === true;

    const choose = (): void =>
    {
        if (selection === "empty")
        {
            onSourceChange?.(patch.slot, sourceRequestForEmpty ());
            return;
        }

        if (selection === "init-tone")
        {
            onSourceChange?.(patch.slot, sourceRequestForInitTone ());
            return;
        }

        const plugin = assignable.find (candidate => candidate.path === selection);

        if (plugin !== undefined)
            onSourceChange?.(patch.slot, sourceRequestForPlugin (plugin));
    };

    return (
        <section className="instrument__card instrument__source-card"
                 aria-label="Instrument source"
                 data-testid="instrument-source-card">
            <header className="instrument__card-head">
                <h3 className="instrument__card-title">Source</h3>
                <span className="instrument__card-hint">{sourceKindLabel (instrument.kind)}</span>
            </header>

            <p className="instrument__source"
               data-testid="instrument-source"
               data-kind={instrument.kind}
               data-id={instrument.id}
               data-name={instrument.name}>
                <span className="instrument__source-name">{instrument.name}</span>
                <span className="instrument__source-kind">{sourceKindLabel (instrument.kind)}</span>
            </p>

            <dl className="instrument__meta">
                {instrument.pluginFormat !== undefined && instrument.pluginFormat.length > 0
                    ? <div>
                          <dt>Format</dt>
                          <dd data-testid="source-format">{instrument.pluginFormat}</dd>
                      </div>
                    : null}
                {instrument.pluginManufacturer !== undefined && instrument.pluginManufacturer.length > 0
                    ? <div>
                          <dt>Manufacturer</dt>
                          <dd data-testid="source-manufacturer">{instrument.pluginManufacturer}</dd>
                      </div>
                    : null}
                {instrument.pluginVersion !== undefined && instrument.pluginVersion.length > 0
                    ? <div>
                          <dt>Version</dt>
                          <dd data-testid="source-version">{instrument.pluginVersion}</dd>
                      </div>
                    : null}
                {instrument.samplePath !== undefined && instrument.samplePath.length > 0
                    ? <div>
                          <dt>Kit</dt>
                          <dd data-testid="source-sample-path">{instrument.samplePath}</dd>
                      </div>
                    : null}
            </dl>

            <div className="instrument__editor"
                 data-testid="source-editor"
                 data-kind={instrument.kind}
                 data-open={editor ? "true" : "false"}>
                <div className="instrument__editor-actions">
                    <button
                        className="instrument__button"
                        type="button"
                        data-testid="source-open-editor"
                        data-slot={patch.slot}
                        data-busy={editorBusy === true ? "true" : "false"}
                        disabled={! isPlugin || editorBusy === true || onOpenEditor === undefined}
                        onClick={() => { onOpenEditor?.(patch.slot); }}
                    >
                        Open editor
                    </button>
                    {editor
                        ? <button
                              className="instrument__button instrument__button--small"
                              type="button"
                              data-testid="source-close-editor"
                              disabled={editorBusy === true || onCloseEditor === undefined}
                              onClick={() => { onCloseEditor?.(patch.slot); }}
                          >
                              Close editor
                          </button>
                        : null}
                </div>

                <p className="instrument__field-hint" data-testid="source-editor-hint">
                    {isPlugin
                        ? "Opens the plug-in's native window on the engine desktop. Selecting or arming this patch never opens it on its own."
                        : "This source is generated by the engine; there are no plug-in parameters to edit."}
                </p>

                {editorError !== null && editorError !== undefined && editorError.length > 0
                    ? <p className="instrument__error"
                          role="alert"
                          data-testid="source-editor-error">
                          {editorError}
                      </p>
                    : null}
            </div>

            <div className="instrument__save"
                 data-testid="patch-state-save"
                 data-slot={patch.slot}
                 data-enabled={isPlugin ? "true" : "false"}
                 data-busy={captureBusy === true ? "true" : "false"}>
                <button
                    className="instrument__button instrument__button--save"
                    type="button"
                    data-testid="source-save-state"
                    data-slot={patch.slot}
                    disabled={! isPlugin || captureBusy === true || onCaptureState === undefined}
                    onClick={() => { onCaptureState?.(patch.slot); }}
                >
                    Save sound to patch
                </button>

                <p className="instrument__field-hint" data-testid="source-save-hint">
                    {isPlugin
                        ? "Captures the running plug-in's current sound into this patch without switching away."
                        : "Only plug-in sources have a sound state to save."}
                </p>

                {captureBusy === true
                    ? <p className="instrument__field-hint"
                          data-testid="source-save-busy"
                          aria-busy="true">
                          Saving the running sound…
                      </p>
                    : null}

                {captureError !== null && captureError !== undefined && captureError.length > 0
                    ? <p className="instrument__error"
                          role="alert"
                          data-testid="source-save-error">
                          {captureError}
                      </p>
                    : null}

                {capture !== null && capture !== undefined && capture.slot === patch.slot
                    ? <p className="instrument__field-hint instrument__save-status"
                          role="status"
                          data-testid="source-save-status"
                          data-size-bytes={capture.sizeBytes}
                          data-slot={capture.slot}>
                          Saved to patch · {formatStateSize (capture.sizeBytes)}
                      </p>
                    : null}
            </div>

            <details className="instrument__details" data-testid="source-change">
                <summary className="instrument__summary">Change source</summary>
                <div className="instrument__picker">
                    <label className="instrument__field" htmlFor="instrument-source-select">
                        <span className="instrument__field-label">Installed source</span>
                        <select
                            id="instrument-source-select"
                            className="instrument__select"
                            data-testid="source-select"
                            value={selection}
                            onChange={event => { setSelection (event.currentTarget.value); }}
                        >
                            <option value="">Choose a source…</option>
                            <option value="empty">Unassigned (silent)</option>
                            <option value="init-tone">Built-in tone</option>
                            {assignable.map (plugin => (
                                <option key={plugin.id} value={plugin.path}>
                                    {plugin.name} · {plugin.vendor}
                                </option>
                            ))}
                        </select>
                    </label>

                    <button
                        className="instrument__button"
                        type="button"
                        data-testid="source-apply"
                        data-busy={sourceBusy === true ? "true" : "false"}
                        disabled={selection.length === 0 || sourceBusy === true || onSourceChange === undefined}
                        onClick={choose}
                    >
                        Apply source
                    </button>

                    {assignable.length === 0
                        ? <p className="instrument__field-hint" data-testid="source-empty">
                              {pluginError ?? "No instrument plug-ins are installed."}
                          </p>
                        : null}
                </div>
            </details>
        </section>
    );
}

function ChainSection ({ patch, chain, chainError, chainBusy, plugins, onAddInsert, onRemoveInsert, onToggleBypass, onMoveInsert, onRetryChain }: {
    readonly patch: ControlPatch;
    readonly chain?: ControlFxChain | null;
    readonly chainError?: string | null;
    readonly chainBusy?: boolean;
    readonly plugins?: readonly ControlPlugin[] | null;
    readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
    readonly onRemoveInsert?: (slot: number, index: number) => void;
    readonly onToggleBypass?: (slot: number, index: number, bypassed: boolean) => void;
    readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
    readonly onRetryChain?: (slot: number) => void;
}): ReactElement
{
    const [selection, setSelection] = useState<string> (`builtin:${BUILTIN_INSERT_TYPES[0]}`);
    const effects = effectPlugins (plugins ?? []);
    const inserts = chain?.inserts ?? [];

    const add = (): void =>
    {
        if (selection.startsWith ("builtin:"))
        {
            const type = selection.slice ("builtin:".length) as BuiltinInsertType;

            if ((BUILTIN_INSERT_TYPES as readonly string[]).includes (type))
                onAddInsert?.(patch.slot, insertRequestForBuiltin (type));

            return;
        }

        if (selection.startsWith ("plugin:"))
        {
            const plugin = effects.find (candidate => candidate.id === selection.slice ("plugin:".length));

            if (plugin !== undefined)
                onAddInsert?.(patch.slot, insertRequestForPlugin (plugin));
        }
    };

    return (
        <section className="instrument__card instrument__chain-card"
                 aria-label="Insert chain"
                 data-testid="fx-chain"
                 data-slot={patch.slot}
                 data-count={inserts.length}
                 data-latency-samples={chain?.latencySamples ?? 0}
                 data-latency-ms={(chain?.latencyMs ?? 0).toFixed (3)}
                 data-busy={chainBusy === true ? "true" : "false"}>
            <header className="instrument__card-head">
                <h3 className="instrument__card-title">Inserts</h3>
                <span className="instrument__card-hint" data-testid="fx-latency-total">
                    {chain === null || chain === undefined
                        ? "latency unavailable"
                        : latencyReadout (chain.latencySamples, chain.latencyMs)}
                </span>
            </header>

            <dl className="instrument__meta instrument__meta--row">
                <div>
                    <dt>Instrument</dt>
                    <dd data-testid="fx-latency-instrument">
                        {String (chain?.instrumentLatencySamples ?? 0)} smp
                    </dd>
                </div>
                <div>
                    <dt>Inserts</dt>
                    <dd data-testid="fx-latency-inserts">
                        {String (chain?.fxLatencySamples ?? 0)} smp
                    </dd>
                </div>
                <div>
                    <dt>Chain</dt>
                    <dd>{String (inserts.length)}</dd>
                </div>
            </dl>

            {chainError !== null && chainError !== undefined
                ? <div className="instrument__error" data-testid="fx-error" role="alert">
                      <span>{chainError}</span>
                      <button
                          className="instrument__button instrument__button--small"
                          type="button"
                          data-testid="fx-retry"
                          onClick={() => { onRetryChain?.(patch.slot); }}
                      >
                          Retry
                      </button>
                  </div>
                : null}

            {chain === null || chain === undefined
                ? <p className="instrument__empty" data-testid="fx-loading" aria-busy="true">
                      Loading the insert chain…
                  </p>
                : inserts.length === 0
                    ? <p className="instrument__empty" data-testid="fx-empty">
                          No inserts. The instrument goes straight to the bus.
                      </p>
                    : <ol className="instrument__inserts" data-testid="fx-inserts">
                          {inserts.map ((insert, index) => (
                              <li
                                  className={`instrument__insert${insert.bypassed ? " instrument__insert--bypassed" : ""}`}
                                  key={`${insert.id}-${String (index)}`}
                                  data-testid="fx-insert"
                                  data-index={index}
                                  data-type={insert.type}
                                  data-kind={insert.kind}
                                  data-name={insert.name}
                                  data-bypassed={insert.bypassed ? "true" : "false"}
                                  data-wet={insert.wet.toFixed (2)}
                                  data-latency-samples={insert.latencySamples}
                              >
                                  <span className="instrument__insert-index">
                                      {String (index + 1).padStart (2, "0")}
                                  </span>
                                  <span className="instrument__insert-name">
                                      <span className="instrument__insert-type">{insertLabel (insert.type)}</span>
                                      <span className="instrument__insert-id">{insert.name}</span>
                                  </span>
                                  <span className="instrument__insert-wet" data-testid="fx-wet">
                                      {String (wetPercent (insert.wet))}% wet
                                  </span>
                                  <span className="instrument__insert-latency">
                                      {String (insert.bypassed ? 0 : insert.latencySamples)} smp
                                  </span>

                                  <span className="instrument__insert-actions">
                                      <button
                                          className="instrument__icon-button"
                                          type="button"
                                          title={`Move ${insert.name} up`}
                                          aria-label={`Move ${insert.name} up`}
                                          data-testid="fx-move-up"
                                          disabled={index === 0}
                                          onClick={() => { onMoveInsert?.(patch.slot, index, index - 1); }}
                                      >
                                          ↑
                                      </button>
                                      <button
                                          className="instrument__icon-button"
                                          type="button"
                                          title={`Move ${insert.name} down`}
                                          aria-label={`Move ${insert.name} down`}
                                          data-testid="fx-move-down"
                                          disabled={index === inserts.length - 1}
                                          onClick={() => { onMoveInsert?.(patch.slot, index, index + 1); }}
                                      >
                                          ↓
                                      </button>
                                      <button
                                          className={`instrument__toggle instrument__toggle--small${insert.bypassed ? "" : " instrument__toggle--on"}`}
                                          type="button"
                                          aria-pressed={! insert.bypassed}
                                          title={insert.bypassed ? `Enable ${insert.name}` : `Bypass ${insert.name}`}
                                          data-testid="fx-bypass"
                                          onClick={() => { onToggleBypass?.(patch.slot, index, ! insert.bypassed); }}
                                      >
                                          {insert.bypassed ? "Off" : "On"}
                                      </button>
                                      <button
                                          className="instrument__icon-button instrument__icon-button--remove"
                                          type="button"
                                          title={`Remove ${insert.name}`}
                                          aria-label={`Remove ${insert.name}`}
                                          data-testid="fx-remove"
                                          onClick={() => { onRemoveInsert?.(patch.slot, index); }}
                                      >
                                          ×
                                      </button>
                                  </span>
                              </li>
                          ))}
                      </ol>}

            <div className="instrument__add" data-testid="fx-add">
                <label className="instrument__field" htmlFor="instrument-insert-select">
                    <span className="instrument__field-label">Add insert</span>
                    <select
                        id="instrument-insert-select"
                        className="instrument__select"
                        data-testid="fx-add-select"
                        value={selection}
                        onChange={event => { setSelection (event.currentTarget.value); }}
                    >
                        {BUILTIN_INSERT_TYPES.map (type => (
                            <option key={type} value={`builtin:${type}`}>
                                {insertLabel (type)}
                            </option>
                        ))}
                        {effects.map (plugin => (
                            <option key={plugin.id} value={`plugin:${plugin.id}`}>
                                {plugin.name} · {plugin.vendor}
                            </option>
                        ))}
                    </select>
                </label>

                <button
                    className="instrument__button"
                    type="button"
                    data-testid="fx-add-button"
                    disabled={chainBusy === true || onAddInsert === undefined}
                    onClick={add}
                >
                    Add insert
                </button>
            </div>
        </section>
    );
}

/**
    The armed instrument's page.

    It shows the patch's real `/state` data (name, family, colour, warm state,
    chain latency), the mixer controls that write straight back to the engine,
    the instrument source with its editor, and the ordered insert chain with
    add / remove / reorder / bypass.

    The page is a pure function of its props: the store owns the requests and
    the optimistic state, so the same tree renders under `react-dom/server`
    for the headless snapshot test.
*/
export function InstrumentPage ({
    patch,
    armedSlot,
    following = true,
    chain = null,
    chainError = null,
    chainBusy = false,
    sourceBusy = false,
    editorOpen = false,
    editorBusy = false,
    editorError = null,
    captureBusy = false,
    capture = null,
    captureError = null,
    plugins = null,
    pluginError = null,
    loading = false,
    onVolumeChange,
    onPanChange,
    onMuteChange,
    onSoloChange,
    onColourChange,
    onSourceChange,
    onOpenEditor,
    onCloseEditor,
    onCaptureState,
    onAddInsert,
    onRemoveInsert,
    onToggleBypass,
    onMoveInsert,
    onRetryChain,
    onBack,
}: InstrumentPageProps): ReactElement
{
    if (patch === null)
    {
        return (
            <section
                className="instrument instrument--empty"
                data-testid="instrument-page"
                data-slot={-1}
                data-following={following ? "true" : "false"}
                aria-busy={loading ? "true" : "false"}
            >
                <header className="instrument__head">
                    <p className="instrument__eyebrow">Instrument</p>
                    <h2 className="instrument__title" data-testid="instrument-name">
                        {loading ? "Loading the set…" : "No patch selected"}
                    </h2>
                    <p className="instrument__lede">
                        {loading
                            ? "Waiting for the engine to report its state."
                            : "Arm a patch on the Grid page, or press a pad on the Launchpad, to open its instrument."}
                    </p>
                </header>

                {onBack !== undefined
                    ? <button className="instrument__button" type="button" onClick={onBack}>
                          Back to the grid
                      </button>
                    : null}
            </section>
        );
    }

    const armed = patch.slot === armedSlot;
    const colour = colourForIndex (patch.colorIndex);

    return (
        <section
            className="instrument"
            data-testid="instrument-page"
            data-slot={patch.slot}
            data-armed={armed ? "true" : "false"}
            data-following={following ? "true" : "false"}
            data-warm-state={patch.warmState}
            data-colour-index={patch.colorIndex}
        >
            <header className="instrument__head">
                <p className="instrument__eyebrow">
                    Instrument · {patch.familyName}
                </p>
                <h2 className="instrument__title" data-testid="instrument-name">
                    {patch.name}
                </h2>
                <p className="instrument__lede">
                    {patch.familyName} · slot {String (patch.slot + 1).padStart (2, "0")}
                    {armed ? " · armed" : " · not armed"}
                    {following ? " · following the controller" : " · pinned selection"}
                </p>

                <div className="instrument__badges">
                    <span
                        className="instrument__badge instrument__badge--warm"
                        data-testid="instrument-warm-state"
                        data-warm={patch.warm ? "true" : "false"}
                        data-warm-state={patch.warmState}
                    >
                        {patch.warmState}
                    </span>
                    <span
                        className="instrument__badge instrument__badge--latency"
                        data-testid="instrument-latency"
                        data-latency-samples={patch.latencySamples}
                        data-latency-ms={patch.latencyMs.toFixed (3)}
                    >
                        {latencyReadout (patch.latencySamples, patch.latencyMs)}
                    </span>
                    <span
                        className="instrument__badge instrument__badge--colour"
                        data-testid="instrument-colour"
                        data-colour-index={patch.colorIndex}
                        data-colour={colour}
                        style={{ background: colour }}
                    >
                        colour {String (patch.colorIndex + 1)}
                    </span>
                    <button
                        className="instrument__button instrument__button--small"
                        type="button"
                        data-testid="instrument-colour-next"
                        onClick={() => { onColourChange?.(patch.slot, nextColourIndex (patch.colorIndex)); }}
                    >
                        Next colour
                    </button>
                </div>
            </header>

            <div className="instrument__grid">
                <MixSection
                    patch={patch}
                    {...(onVolumeChange !== undefined ? { onVolumeChange } : {})}
                    {...(onPanChange !== undefined ? { onPanChange } : {})}
                    {...(onMuteChange !== undefined ? { onMuteChange } : {})}
                    {...(onSoloChange !== undefined ? { onSoloChange } : {})}
                />
                <SourceSection
                    patch={patch}
                    plugins={plugins}
                    pluginError={pluginError}
                    sourceBusy={sourceBusy}
                    editorOpen={editorOpen}
                    editorBusy={editorBusy}
                    editorError={editorError}
                    captureBusy={captureBusy}
                    capture={capture}
                    captureError={captureError}
                    {...(onSourceChange !== undefined ? { onSourceChange } : {})}
                    {...(onOpenEditor !== undefined ? { onOpenEditor } : {})}
                    {...(onCloseEditor !== undefined ? { onCloseEditor } : {})}
                    {...(onCaptureState !== undefined ? { onCaptureState } : {})}
                />
            </div>

            <ChainSection
                patch={patch}
                chain={chain}
                chainError={chainError}
                chainBusy={chainBusy}
                plugins={plugins}
                {...(onAddInsert !== undefined ? { onAddInsert } : {})}
                {...(onRemoveInsert !== undefined ? { onRemoveInsert } : {})}
                {...(onToggleBypass !== undefined ? { onToggleBypass } : {})}
                {...(onMoveInsert !== undefined ? { onMoveInsert } : {})}
                {...(onRetryChain !== undefined ? { onRetryChain } : {})}
            />

            {onBack !== undefined
                ? <button className="instrument__button" type="button" onClick={onBack}>
                      Back to the grid
                  </button>
                : null}
        </section>
    );
}
