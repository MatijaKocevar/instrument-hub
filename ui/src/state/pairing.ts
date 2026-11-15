import { ApiError } from "../api/client";
import {
  clearPairingToken,
  isPairingCode,
  savePairingToken,
  type TokenStorage,
} from "../features/pairing/model";
import type { ActionContext } from "./actionContext";
import { failureToast, messageOf } from "./errors";

export interface PairingServices {
  readonly storage: TokenStorage | null;
  readonly refresh: () => Promise<boolean>;
}

export class PairingActions {
  constructor(
    private readonly context: ActionContext,
    private readonly services: PairingServices,
  ) {}

  toggleLan = async (enabled: boolean): Promise<boolean> => {
    this.patch({ pairingBusy: true });

    try {
      let pairing = await this.context.api.setLanMode(enabled);
      let autoPairError: string | null = null;

      if (!enabled) {
        this.dropToken();
      } else if (
        this.context.api.token === null &&
        isPairingCode(pairing.code)
      ) {
        for (
          let attempt = 0;
          attempt < 8 && this.context.api.token === null;
          attempt += 1
        ) {
          try {
            const result = await this.context.api.pair(pairing.code);
            this.adoptToken(result.token);

            try {
              pairing = await this.context.api.getPairing();
            } catch {}

            autoPairError = null;
            break;
          } catch (error) {
            autoPairError = messageOf(error);

            const transient =
              !(error instanceof ApiError) ||
              error.status === 503 ||
              error.status === 0;

            if (!transient) break;

            await new Promise((resolve) => setTimeout(resolve, 120));
          }
        }
      }

      this.patch({
        pairing,
        pairingBusy: false,
        unauthorized: false,
      });

      if (enabled) await this.services.refresh();

      if (autoPairError !== null) {
        this.notify({
          severity: "warning",
          title: "Could not pair this browser automatically",
          detail: autoPairError,
          key: "pairing",
        });
      }

      return true;
    } catch (error) {
      this.patch({ pairingBusy: false });
      this.notify(failureToast("Could not change LAN mode", error, "pairing"));
      return false;
    }
  };

  pair = async (code: string): Promise<boolean> => {
    this.patch({ pairingBusy: true });

    try {
      const result = await this.context.api.pair(code);
      this.adoptToken(result.token);

      let pairing = this.snapshot.pairing;

      try {
        pairing = await this.context.api.getPairing();
      } catch {}

      this.patch({
        pairing,
        pairingBusy: false,
        unauthorized: false,
        error: null,
      });

      this.notify({
        severity: "success",
        title: "Tablet paired",
        key: "pairing",
      });

      await this.services.refresh();
      return true;
    } catch (error) {
      this.patch({ pairingBusy: false });
      this.notify(failureToast("Pairing failed", error, "pairing"));
      return false;
    }
  };

  forgetPairing = async (): Promise<boolean> => {
    this.patch({ pairingBusy: true });

    try {
      let pairing = await this.context.api.revokePairing();

      if (pairing.lanEnabled && isPairingCode(pairing.code)) {
        const result = await this.context.api.pair(pairing.code);
        this.adoptToken(result.token);
        pairing = await this.context.api.getPairing();
      } else {
        this.dropToken();
      }

      this.patch({ pairing, pairingBusy: false });
      this.notify({
        severity: "success",
        title: "Pairing forgotten",
        key: "pairing",
      });
      return true;
    } catch (error) {
      this.patch({ pairingBusy: false });
      this.notify(failureToast("Could not forget pairing", error, "pairing"));
      return false;
    }
  };

  private adoptToken(token: string): void {
    this.context.api.setToken(token);
    savePairingToken(token, this.services.storage);
  }

  private dropToken(): void {
    this.context.api.setToken(null);
    clearPairingToken(this.services.storage);
  }

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private get snapshot() {
    return this.context.getSnapshot();
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
