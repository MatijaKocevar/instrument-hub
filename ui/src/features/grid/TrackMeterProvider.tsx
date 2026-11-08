import type { ReactElement, ReactNode } from "react";

import { useMeters } from "./useMeters";
import { TrackMetersContext } from "./TrackMetersContext";

export interface TrackMeterProviderProps {
  readonly url: string;
  readonly children: ReactNode;
}

export function TrackMeterProvider({
  url,
  children,
}: TrackMeterProviderProps): ReactElement {
  const meters = useMeters({
    url,
    reconnectBaseMs: 250,
    reconnectMaxMs: 2000,
  });

  return (
    <TrackMetersContext.Provider value={meters}>
      {children}
    </TrackMetersContext.Provider>
  );
}
