import { defineConfig } from "astro/config";
import icon from "astro-icon";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  integrations: [icon()],
  vite: {
    plugins: [tailwindcss()],
  },
  site: "https://github.com/rudironsoni/orlix",
  output: "static",
  build: {
    format: "directory",
  },
});
