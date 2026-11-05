import { defineConfig } from "vite";
import type { Plugin } from "vite";
import react from "@vitejs/plugin-react";

const uiRouteFallback: Plugin = {
  name: "ui-route-fallback",
  configureServer(server) {
    server.middlewares.use((request, _response, next) => {
      const path = request.url?.split("?")[0] ?? "";
      const htmlNavigation = (request.headers.accept ?? "").includes(
        "text/html",
      );
      const uiRoute =
        path === "/audio" || path === "/plugins" || path === "/settings";

      if (request.method === "GET" && uiRoute && htmlNavigation)
        request.url = "/";

      next();
    });
  },
};

export default defineConfig({
  plugins: [react(), uiRouteFallback],
  base: "./",
  server: {
    host: "0.0.0.0",
    port: 5173,
    strictPort: true,
    proxy: {
      "/state": "http://127.0.0.1:8080",
      "/tracks": "http://127.0.0.1:8080",
      "/project": "http://127.0.0.1:8080",
      "/settings": "http://127.0.0.1:8080",
      "/arm": "http://127.0.0.1:8080",
      "/transport": "http://127.0.0.1:8080",
      "/audio": "http://127.0.0.1:8080",
      "/patches": "http://127.0.0.1:8080",
      "/plugins": "http://127.0.0.1:8080",
      "/midi": "http://127.0.0.1:8080",
      "/pairing": "http://127.0.0.1:8080",
      "/pair": "http://127.0.0.1:8080",
      "/meters": { target: "ws://127.0.0.1:8080", ws: true },
      "/events": { target: "ws://127.0.0.1:8080", ws: true },
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
