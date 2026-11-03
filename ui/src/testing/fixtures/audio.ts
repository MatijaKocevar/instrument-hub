import type { ControlAudioState } from "../../api/client";

export const AUDIO: ControlAudioState = {
  open: true,
  deviceType: "ALSA",
  deviceName: "Loopback",
  deviceTypes: ["ALSA"],
  devices: [
    { type: "ALSA", name: "Loopback" },
    { type: "ALSA", name: "UR22mkII" },
  ],
  sampleRate: 48000,
  bufferSize: 128,
  sampleRates: [44100, 48000, 96000],
  bufferSizes: [64, 128, 256, 512],
  inputChannels: [
    { index: 0, name: "channel 1", active: true },
    { index: 1, name: "channel 2", active: true },
  ],
  outputChannels: [
    { index: 0, name: "channel 1", active: true },
    { index: 1, name: "channel 2", active: true },
  ],
  latency: {
    bufferMs: (128 * 1000) / 48000,
    deviceMs: 1.33,
    chainMs: 5,
    totalMs: (128 * 1000) / 48000 + 1.33 + 5,
  },
  restore: {
    usedStoredDevice: true,
    fallback: false,
    reason: "",
  },
  xruns: 2,
  deviceXruns: 1,
  callbackOverruns: 1,
  callbacks: 128000,
  loadPercent: 7.5,
};
