import type { ReactElement } from "react";

import type {
    ControlFxChain,
    ControlFxInsertRequest,
    ControlPatch,
    ControlPatchSourceRequest,
    ControlPatchStateCapture,
    ControlPlugin,
} from "../../api/client";
import { InsertChain } from "./InsertChain";
import { InstrumentFrame } from "./InstrumentFrame";
import { Mixer } from "./Mixer";
import { SaveSoundAction } from "./SaveSoundAction";
import { SourceCard } from "./SourceCard";
import { SourceEditor } from "./SourceEditor";
import { SourcePicker } from "./SourcePicker";
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
}: InstrumentPageProps): ReactElement
{
    return (
        <InstrumentFrame
            patch={patch}
            armedSlot={armedSlot}
            following={following}
            loading={loading}
            {...(onColourChange !== undefined ? { onColourChange } : {})}
        >
            {patch === null
                ? null
                : <>
                      <div className="instrument__grid">
                          <Mixer
                              patch={patch}
                              {...(onVolumeChange !== undefined ? { onVolumeChange } : {})}
                              {...(onPanChange !== undefined ? { onPanChange } : {})}
                              {...(onMuteChange !== undefined ? { onMuteChange } : {})}
                              {...(onSoloChange !== undefined ? { onSoloChange } : {})}
                          />
                          <SourceCard patch={patch}>
                              <SourceEditor
                                  patch={patch}
                                  editorOpen={editorOpen}
                                  editorBusy={editorBusy}
                                  editorError={editorError}
                                  {...(onOpenEditor !== undefined ? { onOpenEditor } : {})}
                                  {...(onCloseEditor !== undefined ? { onCloseEditor } : {})}
                              />

                              <SaveSoundAction
                                  patch={patch}
                                  captureBusy={captureBusy}
                                  capture={capture}
                                  captureError={captureError}
                                  {...(onCaptureState !== undefined ? { onCaptureState } : {})}
                              />

                              <SourcePicker
                                  patch={patch}
                                  plugins={plugins}
                                  pluginError={pluginError}
                                  sourceBusy={sourceBusy}
                                  {...(onSourceChange !== undefined ? { onSourceChange } : {})}
                              />
                          </SourceCard>
                      </div>

                      <InsertChain
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
                  </>}
        </InstrumentFrame>
    );
}
