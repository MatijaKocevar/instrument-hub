import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import App from "./App";
import { createControlApi } from "./api/client";
import { AppStoreProvider } from "./state/AppStoreContext";
import { AppStore } from "./state/store";
import "./styles.css";

// In the embedded web view the engine serves the UI from its own resource
// scheme (not an http origin), so relative requests have nowhere to go. Both
// release and dev therefore default to the documented loopback listener, and
// Vite proxies the API paths to it while developing. `VITE_API_BASE_URL`
// overrides the default when the engine runs on another port or host.
function defaultApiBaseUrl (): string
{
    const protocol = window.location.protocol;

    // A plain-http page (Vite dev server, a detached browser) may talk to its
    // own origin; the embedded resource-provider scheme may not.
    return protocol === "http:" || protocol === "https:"
        ? window.location.origin
        : "http://127.0.0.1:8080";
}

const apiBaseUrl = import.meta.env.VITE_API_BASE_URL ?? defaultApiBaseUrl ();
const token = import.meta.env.VITE_API_TOKEN ?? null;

const store = new AppStore (createControlApi ({ baseUrl: apiBaseUrl, token }));

const container = document.getElementById ("root");

if (container === null)
    throw new Error ("Instrument Hub UI: #root container is missing");

createRoot (container).render (
    <StrictMode>
        <AppStoreProvider store={store}>
            <App />
        </AppStoreProvider>
    </StrictMode>,
);

store.start ();

// A laptop waking from sleep or an engine restart should not wait for the next
// poll: the browser tells us the network came back.
if (typeof window !== "undefined")
{
    window.addEventListener ("online", () => { void store.refresh (); });
    window.addEventListener ("focus", () => { void store.refresh (); });
}
