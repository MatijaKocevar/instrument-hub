import type {
  ControlApi,
  ControlAudioPatch,
  ControlFxInsertRequest,
  ControlFxInsertUpdate,
  ControlMidiInputUpdate,
  ControlPatchSourceRequest,
  ControlPatchUpdate,
  ControlState,
  ControlTransportPatch,
} from "../api/client";
import type { ActionContext } from "./actionContext";
import { AudioActions } from "./audio";
import { FxActions } from "./fx";
import { MidiActions } from "./midi";
import {
  resolveAppStoreOptions,
  type AppStoreOptions,
  type ResolvedAppStoreOptions,
} from "./options";
import { PairingActions } from "./pairing";
import { PatchArmActions } from "./patchArm";
import { PatchCaptureActions } from "./patchCapture";
import { PatchEditorActions } from "./patchEditor";
import { PatchSourceActions } from "./patchSource";
import { PatchUpdateActions } from "./patchUpdate";
import { PluginActions } from "./plugins";
import { PollLoop } from "./pollLoop";
import { ProjectActions } from "./project";
import { refreshSnapshot } from "./refresh";
import type { AppSnapshot } from "./snapshot";
import { StateSync } from "./stateSync";
import { StoreCore } from "./storeCore";
import { TrackActions } from "./track";
import { TransportActions } from "./transport";

export type { AppStoreOptions } from "./options";
export type { AppSnapshot, ConnectionPhase } from "./snapshot";

export class AppStore {
  private readonly api: ControlApi;
  private readonly options: ResolvedAppStoreOptions;
  private readonly core: StoreCore;
  private readonly polling: PollLoop;
  private readonly sync: StateSync;
  private readonly patchArm: PatchArmActions;
  private readonly patchUpdate: PatchUpdateActions;
  private readonly patchSource: PatchSourceActions;
  private readonly patchEditor: PatchEditorActions;
  private readonly patchCapture: PatchCaptureActions;
  private readonly fx: FxActions;
  private readonly plugins: PluginActions;
  private readonly midi: MidiActions;
  private readonly audio: AudioActions;
  private readonly transport: TransportActions;
  private readonly pairing: PairingActions;
  private readonly project: ProjectActions;
  private readonly track: TrackActions;

  constructor(api: ControlApi, options: AppStoreOptions = {}) {
    this.api = api;
    this.options = resolveAppStoreOptions(options);
    this.core = new StoreCore();

    const context: ActionContext = {
      api,
      now: this.options.now,
      getSnapshot: this.core.getSnapshot,
      patch: this.core.patch,
    };

    this.patchArm = new PatchArmActions(context);
    this.patchUpdate = new PatchUpdateActions(context);
    this.fx = new FxActions(context);
    this.patchSource = new PatchSourceActions(context, (slot) =>
      this.fx.loadFxChain(slot, { force: true }),
    );
    this.patchEditor = new PatchEditorActions(context);
    this.patchCapture = new PatchCaptureActions(context);
    this.plugins = new PluginActions(context);
    this.midi = new MidiActions(context);
    this.audio = new AudioActions(context);
    this.transport = new TransportActions(context, (slot) =>
      this.patchArm.armSlot(slot),
    );
    this.pairing = new PairingActions(context, {
      storage: this.options.storage,
      refresh: () => this.refresh(),
    });
    this.project = new ProjectActions(context);
    this.track = new TrackActions(context);

    this.polling = new PollLoop(
      this.options,
      () => this.load(),
      () => this.core.getSnapshot().failedAttempts,
    );

    this.sync = new StateSync({
      baseUrl: api.baseUrl,
      socketFactory: this.options.stateSocketFactory,
      reconnectBaseMs: this.options.retryBaseMs,
      reconnectMaxMs: this.options.retryMaxMs,
      currentState: () => this.core.getSnapshot().state,
      applyState: (state) => {
        this.core.patch({
          phase: "ready",
          state: this.withPendingState(state),
          error: null,
          unauthorized: false,
          failedAttempts: 0,
          updatedAt: this.options.now(),
        });
      },
      onLiveChange: (live) => {
        if (this.core.getSnapshot().live !== live) this.core.patch({ live });
      },
      onAuxiliaryChange: () => {
        void this.refresh();
      },
    });
  }

  getSnapshot = (): AppSnapshot => this.core.getSnapshot();

  get baseUrl(): string {
    return this.api.baseUrl;
  }

  get token(): string | null {
    return this.api.token;
  }

  subscribe = (listener: () => void): (() => void) =>
    this.core.subscribe(listener);

  refresh = (): Promise<boolean> => this.polling.refresh();

  start(): void {
    this.polling.start();
    this.sync.start();
  }

  stop(): void {
    this.sync.stop();
    this.polling.stop();
  }

  retryDelayMs(): number {
    return this.polling.retryDelayMs();
  }

  armSlot = (slot: number | null): Promise<boolean> =>
    this.patchArm.armSlot(slot);

  selectTrack = (index: number): Promise<boolean> =>
    this.track.selectTrack(index);

  toggleTrackFx = (index: number, hot: number): Promise<boolean> =>
    this.track.toggleTrackFx(index, hot);

  setTrackVolume = (index: number, step: number): Promise<boolean> =>
    this.track.setTrackVolume(index, step);

  assignTrackHotFx = (
    index: number,
    hot: number,
    insertId: string | null,
  ): Promise<boolean> => this.track.assignTrackHotFx(index, hot, insertId);

  updatePatch = (slot: number, patch: ControlPatchUpdate): Promise<boolean> =>
    this.patchUpdate.updatePatch(slot, patch);

  updateFamilyColour = (
    familyId: string,
    colorIndex: number,
  ): Promise<boolean> =>
    this.patchUpdate.updateFamilyColour(familyId, colorIndex);

  changeInstrumentSource = (
    slot: number,
    source: ControlPatchSourceRequest,
  ): Promise<boolean> => this.patchSource.changeInstrumentSource(slot, source);

  openInstrumentEditor = (slot: number): Promise<boolean> =>
    this.patchEditor.openInstrumentEditor(slot);

  closeInstrumentEditor = (slot: number): Promise<boolean> =>
    this.patchEditor.closeInstrumentEditor(slot);

  savePatchSound = (slot: number): Promise<boolean> =>
    this.patchCapture.savePatchSound(slot);

  loadProject = (): Promise<boolean> => this.project.loadProject();

  newProject = (path?: string, name?: string): Promise<boolean> =>
    this.project.newProject(path, name);

  openProject = (path: string): Promise<boolean> =>
    this.project.openProject(path);

  saveProject = (): Promise<boolean> => this.project.saveProject();

  saveProjectAs = (path: string): Promise<boolean> =>
    this.project.saveProjectAs(path);

  closeProject = (): Promise<boolean> => this.project.closeProject();

  loadFxChain = (
    slot: number,
    options: { force?: boolean } = {},
  ): Promise<boolean> => this.fx.loadFxChain(slot, options);

  addFxInsert = (
    slot: number,
    insert: ControlFxInsertRequest,
  ): Promise<boolean> => this.fx.addFxInsert(slot, insert);

  updateFxInsert = (
    slot: number,
    index: number,
    update: ControlFxInsertUpdate,
  ): Promise<boolean> => this.fx.updateFxInsert(slot, index, update);

  removeFxInsert = (slot: number, index: number): Promise<boolean> =>
    this.fx.removeFxInsert(slot, index);

  moveFxInsert = (slot: number, from: number, to: number): Promise<boolean> =>
    this.fx.moveFxInsert(slot, from, to);

  replaceFxChain = (
    slot: number,
    inserts: readonly ControlFxInsertRequest[],
  ): Promise<boolean> => this.fx.replaceFxChain(slot, inserts);

  loadPlugins = (search?: string): Promise<boolean> =>
    this.plugins.loadPlugins(search);

  rescanPlugins = (): Promise<boolean> => this.plugins.rescanPlugins();

  loadMidi = (): Promise<boolean> => this.midi.loadMidi();

  refreshMidi = (): Promise<boolean> => this.midi.refreshMidi();

  updateMidiInput = (patch: ControlMidiInputUpdate): Promise<boolean> =>
    this.midi.updateMidiInput(patch);

  toggleMidiInput = (id: string, enabled: boolean): Promise<boolean> =>
    this.midi.toggleMidiInput(id, enabled);

  runPadTest = (): Promise<boolean> => this.midi.runPadTest();

  updateTransport = (patch: ControlTransportPatch): Promise<boolean> =>
    this.transport.updateTransport(patch);

  panic = (): Promise<boolean> => this.transport.panic();

  updateAudio = (patch: ControlAudioPatch): Promise<boolean> =>
    this.audio.updateAudio(patch);

  measureLatency = (
    source: "loopback" | "device" = "loopback",
  ): Promise<boolean> => this.audio.measureLatency(source);

  recommendAudioSetup = (): Promise<boolean> =>
    this.audio.recommendAudioSetup();

  applyAudioSetup = (): Promise<boolean> => this.audio.applyAudioSetup();

  dismissAudioWizard = (): void => this.audio.dismissAudioWizard();

  toggleLan = (enabled: boolean): Promise<boolean> =>
    this.pairing.toggleLan(enabled);

  pair = (code: string): Promise<boolean> => this.pairing.pair(code);

  forgetPairing = (): Promise<boolean> => this.pairing.forgetPairing();

  private load(): Promise<boolean> {
    return refreshSnapshot({
      api: this.api,
      now: this.options.now,
      getSnapshot: this.core.getSnapshot,
      patch: this.core.patch,
      preservePending: (state) => this.withPendingState(state),
    });
  }

  private withPendingState(state: ControlState): ControlState {
    let merged = this.patchArm.preservePending(state);

    merged = this.track.preservePending(merged);
    merged = this.patchUpdate.preservePending(merged);

    return this.transport.preservePending(merged);
  }
}
