import { failureCount } from "./harness";
import { legacyGridTest } from "./scenarios/legacyGrid";
import { trackGridModelTest } from "./scenarios/trackGrid";

function main(): void {
  trackGridModelTest();
  legacyGridTest();

  const failures = failureCount();

  if (failures > 0) {
    console.error(`ui-snapshot: ${String(failures)} check(s) failed`);
    process.exitCode = 1;
    return;
  }

  console.log("ui-snapshot: all checks passed");
}

main();
