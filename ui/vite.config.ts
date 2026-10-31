import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Asset names are deliberately stable (no content hash): the engine embeds
// ui/dist with juce_add_binary_data and serves it by path from a resource
// provider, so the C++ side needs names it can rely on across builds.
export default defineConfig({
  plugins: [react()],
  base: "./",
  server: {
    // WebView::initialUrl() opens http://localhost:5173 in Debug builds.
    host: "localhost",
    port: 5173,
    strictPort: true,
    // The engine serves the UI from its own origin in production, so the app
    // talks to `window.location.origin`. In dev, Vite forwards those paths to
    // the control server instead.
    proxy: {
      "/state": "http://127.0.0.1:8080",
      "/arm": "http://127.0.0.1:8080",
      "/settings": "http://127.0.0.1:8080",
      "/transport": "http://127.0.0.1:8080",
      "/audio": "http://127.0.0.1:8080",
      "/meters": { target: "ws://127.0.0.1:8080", ws: true },
    },
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
    target: "es2022",
    rollupOptions: {
      output: {
        entryFileNames: "assets/app.js",
        chunkFileNames: "assets/[name].js",
        assetFileNames: "assets/[name][extname]",
      },
    },
  },
});
