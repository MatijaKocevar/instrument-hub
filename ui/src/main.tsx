import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import App from "./App";
import { createControlApi } from "./api/client";
import { loadPairingToken } from "./features/pairing";
import { RemoteRoute, isRemotePath } from "./features/remote";
import { ToastViewport } from "./features/toasts";
import { AppStoreProvider } from "./state/AppStoreContext";
import { AppStore } from "./state/store";
import "./styles.css";

function defaultApiBaseUrl(): string {
  const protocol = window.location.protocol;

  return protocol === "http:" || protocol === "https:"
    ? window.location.origin
    : "http://127.0.0.1:8080";
}

const apiBaseUrl = import.meta.env.VITE_API_BASE_URL ?? defaultApiBaseUrl();
const token = import.meta.env.VITE_API_TOKEN ?? loadPairingToken();

const isRemoteRoute = isRemotePath(window.location.pathname);

const store = new AppStore(createControlApi({ baseUrl: apiBaseUrl, token }), {
  storage: typeof localStorage === "undefined" ? null : localStorage,
});

const container = document.getElementById("root");

if (container === null)
  throw new Error("Instrument Hub UI: #root container is missing");

createRoot(container).render(
  <StrictMode>
    <AppStoreProvider store={store}>
      {isRemoteRoute ? <RemoteRoute /> : <App />}
      <ToastViewport surface={isRemoteRoute ? "remote" : "desktop"} />
    </AppStoreProvider>
  </StrictMode>,
);

store.start();

if (typeof window !== "undefined") {
  window.addEventListener("online", () => {
    void store.refresh();
  });
  window.addEventListener("focus", () => {
    void store.refresh();
  });
}
