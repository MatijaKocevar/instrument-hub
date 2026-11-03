import type { ControlAudioState } from "../../api/client";

export const AUDIO: ControlAudioState = {
  open: true,
  deviceType: "ALSA",
  deviceName: "Steinberg UR22mkII",
  deviceTypes: ["ALSA"],
  devices: [
    {
      type: "ALSA",
      name: "Steinberg UR22mkII",
      description: "USB audio interface",
      available: true,
    },
    {
      type: "ALSA",
      name: "HD-Audio Generic (analog)",
      description: "Onboard analog output",
      available: true,
    },
    {
      type: "ALSA",
      name: "HD-Audio Generic (HDMI)",
      description: "Onboard HDMI output",
      available: false,
      reason: "in use by another process",
    },
    {
      type: "ALSA",
      name: "System (PipeWire)",
      description: "System mixer",
      available: true,
    },
    {
      type: "ALSA",
      name: "Steinberg UR22mkII",
      description: "USB audio interface",
      available: true,
    },
  ],
  rawDevices: [
    {
      type: "ALSA",
      name: "Steinberg UR22mkII, USB Audio; Direct hardware device without any conversions",
      description: "hw:CARD=UR22mkII,DEV=0",
      available: true,
    },
    {
      type: "ALSA",
      name: "Steinberg UR22mkII, USB Audio; Direct sample mixing device",
      description: "dmix:CARD=UR22mkII,DEV=0",
      available: true,
    },
    {
      type: "ALSA",
      name: "HD-Audio Generic, ALC887-VD Analog; Direct hardware device without any conversions",
      description: "hw:CARD=Generic,DEV=0",
      available: true,
    },
    {
      type: "ALSA",
      name: "HD-Audio Generic, HDMI 0; Direct hardware device without any conversions",
      description: "hw:CARD=Generic,DEV=3",
      available: true,
    },
    {
      type: "ALSA",
      name: "PulseAudio Sound Server",
      description: "pulse",
      available: true,
    },
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

export const AUDIO_FALLBACK: ControlAudioState = {
  ...AUDIO,
  deviceName: "Loopback",
  restore: {
    usedStoredDevice: false,
    fallback: true,
    reason: 'stored device "Steinberg UR22mkII" is not available',
  },
};
