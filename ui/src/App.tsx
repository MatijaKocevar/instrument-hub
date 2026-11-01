import type { ReactElement } from "react";

import { RemoteRoute } from "./features/remote";
import { AppShell } from "./features/shell";
import { useRouter } from "./state/router";

interface AppProps {
  readonly initialPath?: string;
  readonly viewportWidth?: number | undefined;
}

export default function App({
  initialPath,
  viewportWidth,
}: AppProps = {}): ReactElement {
  const router = useRouter(initialPath);

  if (router.route === "remote") return <RemoteRoute />;

  return <AppShell router={router} viewportWidth={viewportWidth} />;
}
