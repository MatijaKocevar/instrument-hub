import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import {
  AudioSettingsPanel,
  forgetDevicePatch,
  storedDevicePatch,
} from "../../features/audio-settings";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { AUDIO, AUDIO_FALLBACK } from "../fixtures";
import { check, renderApp, writeArtifact } from "../harness";

export function audioRestoreModelTest(): void {
  const stored = storedDevicePatch(AUDIO_FALLBACK);

  check(
    stored?.deviceType === "ALSA" && stored.deviceName === "Steinberg UR22mkII",
    "the fallback reason names the stored device",
  );

  const forget = forgetDevicePatch(AUDIO_FALLBACK);

  check(
    forget.deviceType === "ALSA" && forget.deviceName === "Loopback",
    "forgetting keeps the fallback device in use",
  );

  check(
    storedDevicePatch(AUDIO) === null,
    "a clean restore has no stored device to re-apply",
  );

  const fallbackHtml = renderToStaticMarkup(
    <AudioSettingsPanel audio={AUDIO_FALLBACK} onChange={() => {}} />,
  );

  writeArtifact("ui-audio-fallback.html", fallbackHtml);

  check(
    fallbackHtml.includes('data-testid="audio-restore"'),
    "the fallback notice renders on the audio panel",
  );
  check(
    fallbackHtml.includes('data-fallback="true"'),
    "the notice reports that the stored device was not used",
  );
  check(
    fallbackHtml.includes('data-testid="audio-restore-device">Loopback (ALSA)'),
    "the notice shows the chosen fallback device",
  );
  check(
    fallbackHtml.includes(
      'data-testid="audio-restore-reason">stored device &quot;Steinberg UR22mkII&quot; is not available',
    ),
    "the notice shows the restore reason",
  );
  check(
    fallbackHtml.includes('data-testid="audio-use-stored-device"'),
    "the notice offers to re-apply the stored device",
  );
  check(
    fallbackHtml.includes('data-testid="audio-forget-device"'),
    "the notice offers to forget the stored device",
  );
  check(
    fallbackHtml.includes(">Use this device</button>"),
    "the stored-device action is labelled",
  );
  check(
    fallbackHtml.includes(">Forget device</button>"),
    "the forget action is labelled",
  );

  const cleanHtml = renderToStaticMarkup(
    <AudioSettingsPanel audio={AUDIO} onChange={() => {}} />,
  );

  check(
    !cleanHtml.includes('data-testid="audio-restore"'),
    "a restored device has no fallback notice",
  );
}

export async function audioRestoreActionTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    await store.refresh();
    server.audio.deviceName = "Loopback";
    server.audio.restore = { ...AUDIO_FALLBACK.restore };
    await store.refresh();

    const pageHtml = renderApp(store, "/audio");

    check(
      pageHtml.includes('data-testid="audio-restore"'),
      "the /audio page renders the fallback notice",
    );

    const audio = store.getSnapshot().audio;
    const stored = audio === null ? null : storedDevicePatch(audio);

    check(stored !== null, "the fallback notice knows the stored device");
    check(
      audio !== null && (await store.updateAudio(forgetDevicePatch(audio))),
      "Forget device persists the device in use",
    );
    check(
      server.audioRequests.at(-1)?.deviceName === "Loopback",
      "Forget device posts the fallback device instead of the stored one",
    );

    check(
      stored !== null && (await store.updateAudio(stored)),
      "Use this device re-applies the stored device",
    );
    check(
      server.audioRequests.at(-1)?.deviceName === "Steinberg UR22mkII",
      "Use this device posts the stored device name",
    );
  } finally {
    await server.close();
  }
}
