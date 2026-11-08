import { createContext } from "react";

import { EMPTY_METERS, type MeterSnapshot } from "./meterFrame";

export const TrackMetersContext = createContext<MeterSnapshot>(EMPTY_METERS);
