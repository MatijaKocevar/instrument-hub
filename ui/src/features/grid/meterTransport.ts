import type { MeterSocketFactory, MeterSocketLike } from "./meterSocket";

export interface MeterTransportOptions {
  readonly url: string;
  readonly socketFactory: MeterSocketFactory;
  readonly reconnectBaseMs: number;
  readonly reconnectMaxMs: number;
  readonly onOpen: () => void;
  readonly onMessage: (data: unknown) => void;
  readonly onDisconnect: () => void;
}

export class MeterTransport {
  private readonly options: MeterTransportOptions;
  private socket: MeterSocketLike | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private reconnectDelayMs: number;
  private running = false;

  constructor(options: MeterTransportOptions) {
    this.options = options;
    this.reconnectDelayMs = options.reconnectBaseMs;
  }

  start(): void {
    if (this.running) return;

    this.running = true;
    this.connect();
  }

  stop(): void {
    this.running = false;

    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }

    const socket = this.socket;
    this.socket = null;
    socket?.close();
  }

  private connect(): void {
    if (!this.running || this.options.url.length === 0) return;

    let socket: MeterSocketLike;

    try {
      socket = this.options.socketFactory(this.options.url);
    } catch {
      this.scheduleReconnect();
      return;
    }

    this.socket = socket;

    socket.addEventListener("open", () => {
      if (this.socket !== socket) return;

      this.reconnectDelayMs = this.options.reconnectBaseMs;
      this.options.onOpen();
    });

    socket.addEventListener("message", (event) => {
      if (this.socket !== socket) return;

      this.options.onMessage(event.data);
    });

    const disconnect = (): void => {
      if (this.socket !== socket) return;

      this.socket = null;
      this.options.onDisconnect();
      this.scheduleReconnect();
    };

    socket.addEventListener("close", disconnect);
    socket.addEventListener("error", disconnect);
  }

  private scheduleReconnect(): void {
    if (!this.running || this.reconnectTimer !== null) return;

    const delay = this.reconnectDelayMs;
    this.reconnectDelayMs = Math.min(
      this.options.reconnectMaxMs,
      this.reconnectDelayMs * 2,
    );

    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay);
  }
}
