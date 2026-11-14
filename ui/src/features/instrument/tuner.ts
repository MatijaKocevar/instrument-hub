import type { ControlFxInsertRequest } from "../../api/client";
import { uniqueInsertId } from "./inserts";

export function insertRequestForTuner(
  existingIds: readonly string[],
): ControlFxInsertRequest {
  return {
    type: "tuner",
    id: uniqueInsertId("tuner", existingIds),
    name: "Tuner",
    bypassed: false,
    wet: 1,
    position: -1,
  };
}
