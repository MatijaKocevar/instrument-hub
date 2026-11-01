export interface PollLoopOptions {
  pollIntervalMs: number;
  retryBaseMs: number;
  retryMaxMs: number;
}

export class PollLoop {
  private timer: ReturnType<typeof setTimeout> | null = null;
  private inFlight: Promise<boolean> | null = null;
  private running = false;

  constructor(
    private readonly options: PollLoopOptions,
    private readonly load: () => Promise<boolean>,
    private readonly failedAttempts: () => number,
  ) {}

  refresh = (): Promise<boolean> => {
    if (this.inFlight !== null) return this.inFlight;

    this.inFlight = this.load().finally(() => {
      this.inFlight = null;
    });

    return this.inFlight;
  };

  start(): void {
    if (this.running) return;

    this.running = true;
    this.schedule(0);
  }

  stop(): void {
    this.running = false;

    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
  }

  retryDelayMs(): number {
    const exponent = Math.max(0, this.failedAttempts() - 1);
    const delay = this.options.retryBaseMs * 2 ** exponent;

    return Math.min(this.options.retryMaxMs, delay);
  }

  private schedule(delayMs: number): void {
    if (!this.running) return;

    this.timer = setTimeout(() => {
      this.timer = null;
      void this.tick();
    }, delayMs);
  }

  private async tick(): Promise<void> {
    const ok = await this.refresh();

    if (!this.running) return;

    this.schedule(ok ? this.options.pollIntervalMs : this.retryDelayMs());
  }
}
