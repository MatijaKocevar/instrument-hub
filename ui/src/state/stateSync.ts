import type { ControlState } from "../api/client";
import type { StateSocketFactory } from "./stateSocket";
import { parseStatePushFrame } from "./stateFrame";
import { StateTransport } from "./StateTransport";
import { stateEventsUrlFor } from "./stateUrl";

export interface StateSyncOptions {
  readonly baseUrl: string;
  readonly socketFactory: StateSocketFactory;
  readonly reconnectBaseMs: number;
  readonly reconnectMaxMs: number;
  readonly currentState: () => ControlState | null;
  readonly applyState: (state: ControlState) => void;
  readonly onLiveChange: (live: boolean) => void;
  readonly onAuxiliaryChange: () => void;
}

export class StateSync {
  private readonly transport: StateTransport;

  constructor(private readonly options: StateSyncOptions) {
    this.transport = new StateTransport({
      url: stateEventsUrlFor(options.baseUrl),
      socketFactory: options.socketFactory,
      reconnectBaseMs: options.reconnectBaseMs,
      reconnectMaxMs: options.reconnectMaxMs,
      onOpen: () => {
        this.options.onLiveChange(true);
      },
      onMessage: (data) => {
        this.ingest(data);
      },
      onDisconnect: () => {
        this.options.onLiveChange(false);
      },
    });
  }

  start(): void {
    this.transport.start();
  }

  stop(): void {
    this.transport.stop();
    this.options.onLiveChange(false);
  }

  private ingest(data: unknown): boolean {
    const frame = parseStatePushFrame(data);

    if (frame === null) return false;

    const current = this.options.currentState();
    const fresh =
      current === null ||
      frame.revision > current.revision ||
      frame.state.uptimeMs < current.uptimeMs;

    if (fresh) this.options.applyState(frame.state);

    if (
      frame.kind === "device" ||
      frame.kind === "midi" ||
      frame.kind === "pairing"
    )
      this.options.onAuxiliaryChange();

    return true;
  }
}
