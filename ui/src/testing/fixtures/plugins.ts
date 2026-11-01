// Installed-plug-in inventory fixture served by the snapshot fixture server.

import type { ControlPluginList } from "../../api/client";

/** The installed-plug-in inventory fixture for `GET /plugins`. */
export const PLUGIN_FIXTURE: ControlPluginList = {
    plugins: [
        {
            id: "com.instrumenthub.TestPlugin",
            name: "TestPlugin",
            description: "Instrument Hub test instrument",
            vendor: "Instrument Hub",
            version: "1.0.0",
            category: "Instrument",
            isInstrument: true,
            isEffect: false,
            format: "VST3",
            path: "/usr/lib/vst3/TestPlugin.vst3",
        },
        {
            id: "com.instrumenthub.TestReverb",
            name: "TestReverb",
            description: "Instrument Hub test effect",
            vendor: "Instrument Hub",
            version: "1.0.0",
            category: "Fx",
            isInstrument: false,
            isEffect: true,
            format: "VST3",
            path: "/usr/lib/vst3/TestReverb.vst3",
        },
        {
            id: "com.arturia.AnalogLab",
            name: "Analog Lab V",
            description: "Analog Lab",
            vendor: "Arturia",
            version: "5.10.0",
            category: "Instrument",
            isInstrument: true,
            isEffect: false,
            format: "VST3",
            path: "/usr/lib/vst3/Analog Lab V.vst3",
        },
    ],
    count: 3,
    total: 3,
    search: "",
    scanning: false,
    scanned: true,
    lastScanMs: 1_700_000_000_000,
    error: "",
};
