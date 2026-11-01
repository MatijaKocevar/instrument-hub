import {
    createContext,
    useContext,
    useSyncExternalStore,
    type ReactNode,
} from "react";

import type { AppSnapshot, AppStore } from "./store";

const AppStoreContext = createContext<AppStore | null> (null);

export interface AppStoreProviderProps
{
    store: AppStore;
    children: ReactNode;
}

export function AppStoreProvider ({ store, children }: AppStoreProviderProps)
{
    return (
        <AppStoreContext.Provider value={store}>
            {children}
        </AppStoreContext.Provider>
    );
}

export function useAppStore (): AppStore
{
    const store = useContext (AppStoreContext);

    if (store === null)
        throw new Error ("useAppStore must be used inside <AppStoreProvider>");

    return store;
}

export function useAppState (): AppSnapshot
{
    const store = useAppStore();

    return useSyncExternalStore (store.subscribe, store.getSnapshot, store.getSnapshot);
}
