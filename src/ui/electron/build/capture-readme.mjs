import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync } from "node:child_process";
import { _electron as electron } from "playwright";
import { build } from "vite";
import store from "../shared/overlay-config.cjs";

const ui = path.resolve(fileURLToPath(new URL("..", import.meta.url)));
const root = path.resolve(ui, "../../..");
const output = path.join(root, "docs/media");
const scratch = path.join(root, "work/readme-media");
const frames = path.join(scratch, "frames");
fs.mkdirSync(output, { recursive: true });
fs.mkdirSync(frames, { recursive: true });

await build({
  configFile: false,
  root: ui,
  base: "./",
  logLevel: "error",
  build: {
    outDir: path.join(scratch, "gallery"),
    emptyOutDir: true,
    rolldownOptions: { input: path.join(ui, "build/readme/gallery.html") },
  },
});

const env = { ...process.env };
delete env.ELECTRON_RUN_AS_NODE;
const app = await electron.launch({ args: [ui, "--preview", "--smoke"], env });
const errors = [];
try {
  const page = await app.firstWindow();
  page.on("pageerror", (error) => errors.push(error.message));
  await page
    .getByRole("button", { name: "Change save shortcut, F6", exact: true })
    .waitFor();
  await app.evaluate(() => {
    if (global.sectorTest.hasListener())
      throw new Error("Media capture must stay in preview.");
    global.sectorTest.window().setSize(1000, 720);
    global.sectorTest.window().showInactive();
  });
  const settle = async () => {
    await page.evaluate(() => document.fonts.ready);
    await page.evaluate(() =>
      Promise.all(
        document.getAnimations().map((a) => a.finished.catch(() => {})),
      ),
    );
  };
  await settle();
  await page.screenshot({ path: path.join(output, "active-reset.png") });
  await page.getByRole("tab", { name: "Overlay", exact: true }).click();
  await page.getByRole("button", { name: "Inputs", exact: true }).click();
  await app.evaluate(() => global.sectorTest.window().setSize(1000, 900));
  await settle();
  await page.screenshot({ path: path.join(output, "overlay-editor.png") });
  await app.evaluate(() => global.sectorTest.window().setSize(1000, 720));
  await page.getByRole("button", { name: "Edit", exact: true }).click();
  await page.getByRole("menuitem", { name: "Settings", exact: true }).click();
  await page.getByRole("dialog", { name: "Settings", exact: true }).waitFor();
  await settle();
  await page.screenshot({ path: path.join(output, "settings.png") });

  // Render a gallery of the real components in a separate sandboxed preview window.
  const pending = app.waitForEvent("window");
  await app.evaluate(
    async ({ BrowserWindow }, html) => {
      const window = new BrowserWindow({
        width: 1200,
        height: 1000,
        show: false,
        frame: false,
        webPreferences: {
          sandbox: true,
          contextIsolation: true,
          nodeIntegration: false,
        },
      });
      global.sectorMediaWindow = window;
      await window.loadFile(html);
      window.showInactive();
    },
    path.join(scratch, "gallery/build/readme/gallery.html"),
  );
  const gallery = await pending;
  gallery.on("pageerror", (error) => errors.push(error.message));
  await gallery.waitForFunction(
    () => typeof window.renderSectorMedia === "function",
  );
  const settings = store.defaults().widgets;
  await gallery.evaluate(
    (settings) => window.renderSectorMedia(0, settings),
    settings,
  );
  await gallery.evaluate(() => document.fonts.ready);
  await gallery
    .locator("#showcase")
    .screenshot({ path: path.join(output, "widgets.png") });
  for (let frame = 0; frame < 72; frame++) {
    await gallery.evaluate(
      ({ seconds, settings }) =>
        window.renderSectorMedia(seconds, settings, true),
      {
        seconds: frame / 12,
        settings,
      },
    );
    await gallery.locator("#showcase").screenshot({
      path: path.join(frames, `${String(frame).padStart(3, "0")}.png`),
    });
  }
  if (errors.length) throw new Error(errors.join("\n"));
} finally {
  await app.close();
}

execFileSync(
  "ffmpeg",
  [
    "-hide_banner",
    "-loglevel",
    "error",
    "-y",
    "-framerate",
    "12",
    "-i",
    path.join(frames, "%03d.png"),
    "-frames:v",
    "72",
    "-filter_complex",
    "[0:v]split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3",
    "-loop",
    "0",
    path.join(output, "widgets-demo.gif"),
  ],
  { stdio: "inherit", windowsHide: true },
);
console.log(
  "Captured Sector app screenshots, seven-widget gallery and six-second demo GIF.",
);
