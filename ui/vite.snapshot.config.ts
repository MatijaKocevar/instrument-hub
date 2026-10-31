import { fileURLToPath } from "node:url";
import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// Builds the headless snapshot harness as a single Node ES module. No browser
// is involved: `react-dom/server` renders the tree and the harness asserts on
// the markup, then writes it to tests/artifacts.
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
