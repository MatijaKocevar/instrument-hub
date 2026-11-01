// Pairing fixtures served by the snapshot fixture server.

import type { ControlPairingState } from "../../api/client";

/**
    Pairing fixture. LAN mode starts off, exactly like a fresh engine.
*/

export const PAIRING_OFF: ControlPairingState = {
    lanEnabled: false,
    address: "192.168.1.20",
    port: 8080,
    code: "",
    codeExpiresInMs: 0,
    payload: "",
    pairedClients: 0,
};

export const PAIRING_ON: ControlPairingState = {
    lanEnabled: true,
    address: "192.168.1.20",
    port: 8080,
    code: "246810",
    codeExpiresInMs: 300_000,
    payload: "http://192.168.1.20:8080/remote?code=246810",
    pairedClients: 0,
};
