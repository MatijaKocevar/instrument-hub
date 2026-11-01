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
    readonly patch: ControlPatch | null;
    readonly armedSlot: number;
    readonly following?: boolean;
    readonly chain?: ControlFxChain | null;
    readonly chainError?: string | null;
    readonly chainBusy?: boolean;
    readonly sourceBusy?: boolean;
    readonly editorOpen?: boolean;
    readonly editorBusy?: boolean;
    readonly editorError?: string | null;
    readonly captureBusy?: boolean;
    readonly capture?: ControlPatchStateCapture | null;
    readonly captureError?: string | null;
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
