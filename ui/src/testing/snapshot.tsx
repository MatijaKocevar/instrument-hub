import { failureCount } from "./harness";
import { audioSettingsTest } from "./scenarios/audio";
import {
  armTest,
  colourEditTest,
  gridSnapshotTest,
  loadingTest,
  meterTest,
} from "./scenarios/grid";
import {
  instrumentModelTest,
  instrumentPageTest,
  instrumentPluginEditorTest,
} from "./scenarios/instrument";
import { midiModelTest, midiPageTest } from "./scenarios/midi";
import { pairingTest, qrTest } from "./scenarios/pairing";
import { pluginsModelTest, pluginsPageTest } from "./scenarios/plugins";
import {
  remoteBannerTest,
  remoteContrastTest,
  remoteInteractionTest,
  remoteLinkTest,
  remoteModelTest,
  remoteOptimisticTest,
  remoteSnapshotTest,
  remoteWakeLockTest,
} from "./scenarios/remote";
import {
  clickArmsAndRoutesTest,
  routeTest,
  shellTest,
} from "./scenarios/shell";
import { snapshotTest } from "./scenarios/snapshot";
import { backoffTest, reconnectTest } from "./scenarios/store";

async function main(): Promise<void> {
  await snapshotTest();
  await gridSnapshotTest();
  await armTest();
  routeTest();
  await shellTest();
  await clickArmsAndRoutesTest();
  await colourEditTest();
  meterTest();
  loadingTest();
  await audioSettingsTest();
  midiModelTest();
  await midiPageTest();
  await pairingTest();
  qrTest();
  remoteSnapshotTest();
  remoteModelTest();
  await remoteInteractionTest();
  remoteBannerTest();
  await remoteWakeLockTest();
  remoteContrastTest();
  await remoteLinkTest();
  await remoteOptimisticTest();
  await reconnectTest();
  await backoffTest();
  instrumentModelTest();
  await instrumentPageTest();
  instrumentPluginEditorTest();
  pluginsModelTest();
  await pluginsPageTest();

  const failures = failureCount();

  if (failures > 0) {
    console.error(`ui-snapshot: ${String(failures)} check(s) failed`);
    process.exitCode = 1;
    return;
  }

  console.log("ui-snapshot: all checks passed");
}

main().catch((error) => {
  console.error("ui-snapshot: ", error);
  process.exitCode = 1;
});
