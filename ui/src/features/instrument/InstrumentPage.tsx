import type { ReactElement } from "react";

import type {
  ControlFxChain,
  ControlFxInsertRequest,
  ControlPatch,
  ControlPatchSourceRequest,
  ControlPatchStateCapture,
  ControlPlugin,
  ControlTrack,
} from "../../api/client";
import type { GridTrack } from "../grid";
import { InsertChain } from "./InsertChain";
import { InstrumentFrame } from "./InstrumentFrame";
import { InstrumentTrackStrip } from "./InstrumentTrackStrip";
import { Mixer } from "./Mixer";
import { SaveSoundAction } from "./SaveSoundAction";
import { SourceCard } from "./SourceCard";
import { SourceEditor } from "./SourceEditor";
import { SourcePicker } from "./SourcePicker";
import "./instrument.css";

export interface InstrumentPageProps {
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
  readonly track?: ControlTrack | null;
  readonly gridTrack?: GridTrack | null;
  readonly metersUrl?: string;
  readonly loading?: boolean;

  readonly onPanChange?: (slot: number, pan: number) => void;
  readonly onMuteChange?: (slot: number, muted: boolean) => void;
  readonly onSoloChange?: (slot: number, solo: boolean) => void;
  readonly onSourceChange?: (
    slot: number,
    source: ControlPatchSourceRequest,
  ) => void;
  readonly onOpenEditor?: (slot: number) => void;
  readonly onReloadPlugin?: (slot: number) => void;
  readonly reloadBusy?: boolean;
  readonly onCaptureState?: (slot: number) => void;
  readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
  readonly onRemoveInsert?: (slot: number, index: number) => void;
  readonly onToggleBypass?: (
    slot: number,
    index: number,
    bypassed: boolean,
  ) => void;
  readonly onMoveInsert?: (slot: number, from: number, to: number) => void;
  readonly onRetryChain?: (slot: number) => void;
  readonly onAssignHotFx?: (
    trackIndex: number,
    hot: number,
    insertId: string | null,
  ) => void;
  readonly onRenameTrack?: (trackIndex: number, name: string) => void;
  readonly onSelectTrack?: (trackIndex: number) => void;
  readonly onOpenTrack?: (trackIndex: number) => void;
  readonly onToggleTrackFx?: (trackIndex: number, hot: number) => void;
  readonly onSetTrackVolume?: (trackIndex: number, gain: number) => void;
}

export function InstrumentPage({
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
  track = null,
  gridTrack = null,
  metersUrl = "",
  loading = false,
  onPanChange,
  onMuteChange,
  onSoloChange,
  onSourceChange,
  onOpenEditor,
  onReloadPlugin,
  reloadBusy = false,
  onCaptureState,
  onAddInsert,
  onRemoveInsert,
  onToggleBypass,
  onMoveInsert,
  onRetryChain,
  onAssignHotFx,
  onRenameTrack,
  onSelectTrack,
  onOpenTrack,
  onToggleTrackFx,
  onSetTrackVolume,
}: InstrumentPageProps): ReactElement {
  return (
    <InstrumentFrame
      patch={patch}
      armedSlot={armedSlot}
      following={following}
      loading={loading}
      track={track}
      {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
    >
      {patch === null ? null : (
        <>
          <div className="instrument__workspace">
            <div className="instrument__grid">
              <SourceCard patch={patch}>
                <SourceEditor
                  patch={patch}
                  editorOpen={editorOpen}
                  editorBusy={editorBusy}
                  editorError={editorError}
                  {...(onOpenEditor !== undefined ? { onOpenEditor } : {})}
                  {...(onReloadPlugin !== undefined
                    ? {
                        onReload: () => {
                          onReloadPlugin(patch.slot);
                        },
                      }
                    : {})}
                  reloadBusy={reloadBusy}
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

              <Mixer
                patch={patch}
                {...(onPanChange !== undefined ? { onPanChange } : {})}
                {...(onMuteChange !== undefined ? { onMuteChange } : {})}
                {...(onSoloChange !== undefined ? { onSoloChange } : {})}
              />
            </div>

            {gridTrack !== null ? (
              <InstrumentTrackStrip
                track={gridTrack}
                metersUrl={metersUrl}
                {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
                {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
                {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
                {...(onSetTrackVolume !== undefined
                  ? { onSetTrackVolume }
                  : {})}
                {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
              />
            ) : null}
          </div>

          <InsertChain
            patch={patch}
            chain={chain}
            chainError={chainError}
            chainBusy={chainBusy}
            plugins={plugins}
            track={track}
            {...(onAddInsert !== undefined ? { onAddInsert } : {})}
            {...(onRemoveInsert !== undefined ? { onRemoveInsert } : {})}
            {...(onToggleBypass !== undefined ? { onToggleBypass } : {})}
            {...(onMoveInsert !== undefined ? { onMoveInsert } : {})}
            {...(onRetryChain !== undefined ? { onRetryChain } : {})}
            {...(onAssignHotFx !== undefined ? { onAssignHotFx } : {})}
          />
        </>
      )}
    </InstrumentFrame>
  );
}
