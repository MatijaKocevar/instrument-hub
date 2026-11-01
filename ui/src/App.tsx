import type { ReactElement } from "react";

import { RemoteRoute } from "./features/remote";
import { AppShell } from "./features/shell";
import { useRouter } from "./state/router";

interface AppProps
{
    /** Render a specific route in the headless snapshot test. */
    readonly initialPath?: string;
    /** Viewport width in the headless snapshot test (the browser reads its own). */
    readonly viewportWidth?: number | undefined;
}

export default function App ({ initialPath, viewportWidth }: AppProps = {}): ReactElement
{
    const router = useRouter (initialPath);

    // `/remote` is the tablet surface: it owns the whole window and never gets
    // the desktop sidebar, even if the shell bundle happens to render it.
    if (router.route === "remote")
        return <RemoteRoute />;

    return <AppShell router={router} viewportWidth={viewportWidth} />;
}
