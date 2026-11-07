import { fileURLToPath } from "node:url";
import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [react()],
  build: {
    ssr: fileURLToPath(new URL("./src/testing/snapshot.tsx", import.meta.url)),
    outDir: "dist-test",
    emptyOutDir: true,
    target: "node22",
    minify: false,
    rollupOptions: {
      output: {
        entryFileNames: "snapshot.mjs",
        format: "es",
      },
    },
  },
});
