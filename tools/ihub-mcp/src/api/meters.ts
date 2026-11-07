import WebSocket from "ws";
import { z } from "zod";
import type { HubConfig } from "../config.js";
import { websocketUrl } from "../config.js";
import type { components } from "./schema.js";

export type MeterFrame = components["schemas"]["MeterFrame"];

const meterFrameSchema: z.ZodType<MeterFrame> = z.object({
  type: z.literal("meters"),
  sequence: z.number().int().positive(),
  timestampMs: z.number(),
  master: z.object({ peak: z.number().min(0).max(1) }),
  slots: z.array(
    z.object({
      slot: z.number().int().min(0),
      peak: z.number().min(0).max(1),
      gain: z.number().min(0).max(1),
      armed: z.boolean(),
    }),
  ),
});

export function readLatestMeterFrame(
  config: HubConfig,
  timeoutMs = 5000,
): Promise<MeterFrame> {
  return new Promise((resolve, reject) => {
    const options = config.token
      ? { headers: { "X-Instrument-Hub-Token": config.token } }
      : {};
    const socket = new WebSocket(
      websocketUrl(config.apiUrl, "meters"),
      options,
    );
    let settled = false;
    let timer: NodeJS.Timeout;

    const finish = (action: () => void) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      socket.removeAllListeners();
      socket.close();
      action();
    };

    timer = setTimeout(
      () => finish(() => reject(new Error(`no meter frame within ${timeoutMs} ms`))),
      timeoutMs,
    );

    socket.on("message", (data) => {
      try {
        const frame = meterFrameSchema.parse(JSON.parse(data.toString()));
        finish(() => resolve(frame));
      } catch (error) {
        finish(() => reject(asError(error)));
      }
    });
    socket.on("error", (error) => finish(() => reject(error)));
    socket.on("close", (code, reason) => {
      finish(() =>
        reject(
          new Error(
            `meter socket closed before a frame (${code}: ${reason.toString()})`,
          ),
        ),
      );
    });
  });
}

function asError(value: unknown): Error {
  return value instanceof Error ? value : new Error(String(value));
}
