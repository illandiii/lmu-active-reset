import { defineConfig } from "vite";
import { fileURLToPath } from "node:url";

export default defineConfig({
  base: "./",
  resolve: {
    alias: {
      "@assets": fileURLToPath(new URL("../../../assets", import.meta.url)),
    },
  },
  build: {
    target: "chrome144",
    outDir: "dist",
    emptyOutDir: true,
    rolldownOptions: {
      onwarn(warning, defaultHandler) {
        // React client directives are redundant in this entirely client-side app.
        if (
          warning.code === "MODULE_LEVEL_DIRECTIVE" &&
          warning.message.includes("use client")
        )
          return;
        defaultHandler(warning);
      },
    },
  },
});
