import type { IncomingMessage, ServerResponse } from "node:http";

import type {
  ControlAudioPatch,
  ControlAudioState,
  ControlFxChain,
  ControlMidiState,
  ControlPairingState,
  ControlPlugin,
  ControlProjectOverview,
  ControlState,
  ControlTransportPatch,
} from "../../api/client";

export interface FixtureTrackRequest {
  readonly kind: "select" | "toggle" | "volume" | "assign";
  readonly index: number;
  readonly hot?: number;
  readonly step?: number;
  readonly insertId?: string;
}

export interface FixtureServer {
  baseUrl: string;
  readonly stateRequests: number;
  readonly armRequests: readonly { slot: number | null }[];
  readonly trackRequests: readonly FixtureTrackRequest[];
  readonly audioRequests: readonly ControlAudioPatch[];
  readonly transportRequests: readonly ControlTransportPatch[];
  readonly measureRequests: number;
  readonly audio: ControlAudioState;
  readonly pairing: ControlPairingState;
  readonly pairRequests: readonly string[];
  readonly lanRequests: readonly boolean[];
  readonly patchRequests: readonly {
    slot: number;
    patch: Record<string, unknown>;
  }[];
  readonly fxRequests: readonly {
    method: string;
    slot: number;
    index: number;
    body: Record<string, unknown>;
  }[];
  readonly sourceRequests: readonly {
    slot: number;
    body: Record<string, unknown>;
  }[];
  readonly editorRequests: readonly { slot: number; open: boolean }[];
  readonly stateCaptureRequests: readonly { slot: number }[];
  readonly pluginRequests: readonly string[];
  readonly midiRequests: readonly { id: string; enabled: boolean }[];
  readonly padTestRequests: number;
  readonly midi: ControlMidiState;
  readonly chain: ControlFxChain;
  readonly project: ControlProjectOverview;
  readonly projectRequests: readonly { action: string; path: string }[];
  close: () => Promise<void>;
}

export interface FixtureOptions {
  failFirst?: number;
  armDelayMs?: number;
  layout?: "families" | "tracks";
}

export interface FixtureContext {
  readonly failFirst: number;
  readonly armDelayMs: number;
  readonly state: ControlState;
  readonly audio: ControlAudioState;
  readonly midi: ControlMidiState;
  readonly chains: Map<number, ControlFxChain>;

  project: ControlProjectOverview;
  pairing: ControlPairingState;
  pairingToken: string | null;
  pluginPlugins: ControlPlugin[];
  pluginScanning: boolean;
  pluginPendingScan: boolean;

  stateRequests: number;
  measureRequests: number;
  padTestRequests: number;

  readonly armRequests: { slot: number | null }[];
  readonly trackRequests: FixtureTrackRequest[];
  readonly audioRequests: ControlAudioPatch[];
  readonly transportRequests: ControlTransportPatch[];
  readonly pairRequests: string[];
  readonly lanRequests: boolean[];
  readonly patchRequests: { slot: number; patch: Record<string, unknown> }[];
  readonly fxRequests: {
    method: string;
    slot: number;
    index: number;
    body: Record<string, unknown>;
  }[];
  readonly sourceRequests: { slot: number; body: Record<string, unknown> }[];
  readonly editorRequests: { slot: number; open: boolean }[];
  readonly stateCaptureRequests: { slot: number }[];
  readonly pluginRequests: string[];
  readonly midiRequests: { id: string; enabled: boolean }[];

  readonly projectRequests: { action: string; path: string }[];

  chainFor(slot: number): ControlFxChain;
}

export type FixtureRoute = (
  context: FixtureContext,
  request: IncomingMessage,
  response: ServerResponse,
) => boolean;
