import { AUDIO_WIZARD } from "../../fixtures/audio";
import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleAudioWizard: FixtureRoute = (context, request, response) => {
  if (request.method === "POST" && request.url === "/audio/wizard") {
    context.wizardRequests += 1;
    sendJson(response, 200, AUDIO_WIZARD);
    return true;
  }

  if (request.method === "POST" && request.url === "/audio/wizard/apply") {
    context.wizardApplyRequests += 1;

    context.audio.deviceType = AUDIO_WIZARD.deviceType;
    context.audio.deviceName = AUDIO_WIZARD.deviceName;
    context.audio.sampleRate = AUDIO_WIZARD.sampleRate;
    context.audio.bufferSize = AUDIO_WIZARD.bufferSize;
    context.audio.inputChannels = context.audio.inputChannels.map(
      (channel) => ({
        ...channel,
        active: AUDIO_WIZARD.inputChannels.includes(channel.index),
      }),
    );
    context.audio.outputChannels = context.audio.outputChannels.map(
      (channel) => ({
        ...channel,
        active: AUDIO_WIZARD.outputChannels.includes(channel.index),
      }),
    );
    context.audio.profiles = [
      {
        soundcardKey: AUDIO_WIZARD.soundcard,
        deviceType: AUDIO_WIZARD.deviceType,
        deviceName: AUDIO_WIZARD.deviceName,
        sampleRate: AUDIO_WIZARD.sampleRate,
        bufferSize: AUDIO_WIZARD.bufferSize,
        inputChannels: AUDIO_WIZARD.inputChannels,
        outputChannels: AUDIO_WIZARD.outputChannels,
        profile: AUDIO_WIZARD.profile,
        latencyMs: AUDIO_WIZARD.latencyMs,
        measuredMs: AUDIO_WIZARD.measuredMs,
        xruns: AUDIO_WIZARD.xruns,
      },
    ];

    sendJson(response, 200, context.audio);
    return true;
  }

  return false;
};
