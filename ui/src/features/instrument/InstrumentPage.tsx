import type { ReactElement } from "react";

import type {
  ControlAudioChannel,
  ControlFxChain,
  ControlFxInsertRequest,
  ControlFxInsertUpdate,
  ControlFxTunerState,
  ControlPatch,
  ControlPatchSourceRequest,
  ControlPlugin,
  ControlTrack,
} from "../../api/client";
import type { GridTrack } from "../grid";
import { InsertChain } from "./InsertChain";
import { InstrumentFrame } from "./InstrumentFrame";
import { InstrumentTrackStrip } from "./InstrumentTrackStrip";
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
  readonly chainUnavailable?: boolean;
  readonly chainBusy?: boolean;
  readonly sourceBusy?: boolean;
  readonly editorOpen?: boolean;
  readonly editorBusy?: boolean;
  readonly captureBusy?: boolean;
  readonly plugins?: readonly ControlPlugin[] | null;
  readonly pluginsUnavailable?: boolean;
  readonly audioInputs?: readonly ControlAudioChannel[] | null;
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
  readonly onUpdateInsert?: (
    slot: number,
    index: number,
    update: ControlFxInsertUpdate,
  ) => void;
  readonly onReadTuner?: (
    slot: number,
    index: number,
  ) => Promise<ControlFxTunerState>;
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
  chainUnavailable = false,
  chainBusy = false,
  sourceBusy = false,
  editorOpen = false,
  editorBusy = false,
  captureBusy = false,
  plugins = null,
  pluginsUnavailable = false,
  audioInputs = null,
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
  onUpdateInsert,
  onReadTuner,
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
                <div className="instrument__source-actions">
                  <SourceEditor
                    patch={patch}
                    editorOpen={editorOpen}
                    editorBusy={editorBusy}
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
                    {...(onCaptureState !== undefined
                      ? { onCaptureState }
                      : {})}
                  />
                </div>

                <SourcePicker
                  patch={patch}
                  plugins={plugins}
                  pluginsUnavailable={pluginsUnavailable}
                  audioInputs={audioInputs}
                  sourceBusy={sourceBusy}
                  {...(onSourceChange !== undefined ? { onSourceChange } : {})}
                />
              </SourceCard>

              <InsertChain
                patch={patch}
                chain={chain}
                chainUnavailable={chainUnavailable}
                chainBusy={chainBusy}
                plugins={plugins}
                track={track}
                {...(onAddInsert !== undefined ? { onAddInsert } : {})}
                {...(onRemoveInsert !== undefined ? { onRemoveInsert } : {})}
                {...(onToggleBypass !== undefined ? { onToggleBypass } : {})}
                {...(onMoveInsert !== undefined ? { onMoveInsert } : {})}
                {...(onUpdateInsert !== undefined ? { onUpdateInsert } : {})}
                {...(onReadTuner !== undefined ? { onReadTuner } : {})}
                active={patch.slot === armedSlot}
                {...(onRetryChain !== undefined ? { onRetryChain } : {})}
                {...(onAssignHotFx !== undefined ? { onAssignHotFx } : {})}
              />
            </div>

            {gridTrack !== null ? (
              <InstrumentTrackStrip
                track={gridTrack}
                patch={patch}
                metersUrl={metersUrl}
                {...(onSelectTrack !== undefined ? { onSelectTrack } : {})}
                {...(onOpenTrack !== undefined ? { onOpenTrack } : {})}
                {...(onToggleTrackFx !== undefined ? { onToggleTrackFx } : {})}
                {...(onSetTrackVolume !== undefined
                  ? { onSetTrackVolume }
                  : {})}
                {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
                {...(onPanChange !== undefined ? { onPanChange } : {})}
                {...(onMuteChange !== undefined ? { onMuteChange } : {})}
                {...(onSoloChange !== undefined ? { onSoloChange } : {})}
              />
            ) : null}
          </div>
        </>
      )}
    </InstrumentFrame>
  );
}
