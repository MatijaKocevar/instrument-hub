import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import {
  AudioSettingsPanel,
  bufferSizePatch,
} from "../../features/audio-settings";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { check, countOf, renderApp } from "../harness";

export async function audioSettingsTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    const loaded = await store.refresh();
    check(loaded, "audio fixture loads with GET /state + GET /audio");
    check(
      store.getSnapshot().audio?.bufferSize === 128,
      "store holds the fetched 128-frame buffer",
    );
    check(
      store.getSnapshot().audio?.latency.bufferMs === (128 * 1000) / 48000,
      "fetched buffer latency is one buffer at 48 kHz",
    );

    const initial = renderApp(store, "/audio");

    check(
      initial.includes('data-buffer-size="128"'),
      "panel renders the fetched buffer size",
    );
    check(
      initial.includes('data-testid="latency-buffer">2.67 ms'),
      "buffer latency readout shows 2.67 ms at 128 frames / 48 kHz",
    );
    check(
      initial.includes('data-testid="latency-chain">5.00 ms'),
      "chain latency readout shows the summed chain total",
    );
    check(
      initial.includes('data-testid="latency-total">9.00 ms'),
      "total latency is buffer + device + chain",
    );
    check(
      initial.includes('data-testid="xrun-count">2<'),
      "xrun counter shows the device + callback total",
    );

    const changed = await store.updateAudio(bufferSizePatch(256));

    check(changed, "changing the buffer size succeeds");
    check(
      server.audioRequests.length === 1 &&
        server.audioRequests[0]?.bufferSize === 256,
      "the UI sent { bufferSize: 256 } to POST /audio",
    );
    check(
      server.audio.bufferSize === 256,
      "the engine fixture applied 256 frames",
    );
    check(
      store.getSnapshot().audio?.bufferSize === 256,
      "store adopts the post-change buffer size",
    );

    const after = renderApp(store, "/audio");

    check(
      after.includes('data-buffer-size="256"'),
      "the readout updates to 256 frames",
    );
    check(
      after.includes('data-testid="latency-buffer">5.33 ms'),
      "buffer latency updates to 5.33 ms at 256 frames / 48 kHz",
    );
    check(
      after.includes('data-testid="latency-total">11.7 ms'),
      "total latency readout updates with the buffer size",
    );
    check(
      after.includes("256 frames"),
      "the buffer control lists the new current value",
    );

    const measured = await store.measureLatency();
    const measurement = store.getSnapshot().measurement;

    check(
      measured && measurement?.valid === true,
      "Measure returns a valid loopback measurement",
    );
    check(
      server.measureRequests === 1,
      "the Measure button path called POST /audio/measure",
    );
    check(
      measurement !== null && Math.abs(measurement.roundTripMs - 10.6667) < 0.1,
      "measured round trip is two buffers at 256 frames (10.7 ms)",
    );

    const measuredHtml = renderApp(store, "/audio");

    check(
      measuredHtml.includes('data-testid="measure-result" data-valid="true"'),
      "the panel renders the successful measurement",
    );
    check(
      measuredHtml.includes("10.7 ms"),
      "the panel shows the measured round-trip in milliseconds",
    );
    check(
      measuredHtml.includes("512 samples"),
      "the panel shows the measured sample count",
    );

    const remapped = await store.updateAudio({ outputChannels: [0] });

    check(
      remapped &&
        store.getSnapshot().audio?.outputChannels[1]?.active === false,
      "channel mapping changes reach the engine and come back",
    );

    const rejected = await store.updateAudio({ bufferSize: 0 });

    check(!rejected, "an invalid buffer size is rejected");
    check(
      store.getSnapshot().audioError?.includes("bufferSize") === true,
      "the panel surfaces the engine rejection",
    );

    const loadingHtml = renderToStaticMarkup(
      <AudioSettingsPanel audio={null} loading />,
    );

    check(
      loadingHtml.includes('data-testid="audio-settings-loading"'),
      "audio panel shows a loading state before the first GET /audio",
    );
    check(
      loadingHtml.includes('aria-busy="true"'),
      "loading audio panel is marked busy",
    );
  } finally {
    await server.close();
  }
}

export function assertSnapshotAudio(audioHtml: string): void {
  check(
    audioHtml.includes('data-testid="audio-page"'),
    "the /audio route resolves to its page",
  );
  check(
    audioHtml.includes('data-testid="audio-settings"'),
    "audio page shows the audio settings panel",
  );
  check(
    audioHtml.includes('data-testid="device-type-select"'),
    "panel shows the device type control",
  );
  check(
    audioHtml.includes('data-testid="device-select"'),
    "panel shows the raw device control",
  );
  check(
    countOf(audioHtml, 'data-testid="device-card"') === 4,
    "the picker shows four device cards",
  );
  check(
    countOf(audioHtml, 'data-device="Steinberg UR22mkII"') === 1,
    "the picker has no duplicate card rows",
  );
  check(
    audioHtml.includes('data-testid="device-reason">in use by another process'),
    "an unavailable card shows its reason",
  );
  check(
    audioHtml.includes('data-testid="device-advanced-toggle"'),
    "the picker offers the Advanced toggle",
  );
  check(
    audioHtml.includes(
      'data-testid="device-advanced" data-open="false" hidden',
    ),
    "the raw PCM list starts hidden",
  );
  check(
    audioHtml.includes('data-testid="sample-rate-select"'),
    "panel shows the sample rate control",
  );
  check(
    audioHtml.includes('data-testid="buffer-size-select"'),
    "panel shows the buffer size control",
  );
  check(
    audioHtml.includes('data-testid="latency-buffer"'),
    "panel shows the buffer latency",
  );
  check(
    audioHtml.includes('data-testid="latency-device"'),
    "panel shows the device latency",
  );
  check(
    audioHtml.includes('data-testid="latency-chain"'),
    "panel shows the chain latency",
  );
  check(
    audioHtml.includes('data-testid="latency-total"'),
    "panel shows the total latency",
  );
  check(
    audioHtml.includes('data-testid="xrun-count"'),
    "panel shows the xrun counter",
  );
  check(
    audioHtml.includes('data-testid="measure-button"'),
    "panel shows the Measure button",
  );
  check(
    audioHtml.includes('data-open="true"'),
    "panel reports the device is open",
  );
  check(
    audioHtml.includes('data-testid="midi-panel"'),
    "the audio page shows the MIDI panel",
  );
  check(
    audioHtml.includes('data-testid="pad-test"'),
    "the MIDI panel has a pad test",
  );
  check(
    audioHtml.includes('data-testid="launchpad-status"'),
    "the MIDI panel shows the Launchpad status",
  );
  check(
    audioHtml.includes('data-testid="keystation-status"'),
    "the MIDI panel shows the Keystation status",
  );
}

export function assertSnapshotAudioChannels(
  audioHtml: string,
  store: AppStore,
): void {
  check(
    countOf(audioHtml, 'data-testid="output-channel"') ===
      (store.getSnapshot().audio?.outputChannels.length ?? 0),
    "panel maps every output channel the engine reports",
  );
}
