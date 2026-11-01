import type { ReactElement } from "react";

import type { RouterState } from "../../state/router";
import { AudioRoute } from "./AudioRoute";
import { GridRoute } from "./GridRoute";
import { InstrumentRoute } from "./InstrumentRoute";
import { PluginsRoute } from "./PluginsRoute";
import { SettingsRoute } from "./SettingsRoute";
import type { ShellData } from "./useShellData";

export function RouteOutlet({
  router,
  shell,
}: {
  readonly router: RouterState;
  readonly shell: ShellData;
}): ReactElement {
  if (router.route === "grid")
    return <GridRoute router={router} shell={shell} />;

  if (router.route === "instrument") return <InstrumentRoute shell={shell} />;

  if (router.route === "plugins") return <PluginsRoute shell={shell} />;

  if (router.route === "settings") return <SettingsRoute shell={shell} />;

  return <AudioRoute shell={shell} />;
}
