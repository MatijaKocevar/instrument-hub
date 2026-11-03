import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  base: "./",
  server: {
    host: "localhost",
    port: 5173,
    strictPort: true,
    proxy: {
      "/state": "http://127.0.0.1:8080",
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
