import { _electron as electron } from "playwright";
import assert from "node:assert/strict";
import path from "node:path";
import fs from "node:fs";
import { fileURLToPath } from "node:url";
const directory = path.resolve(fileURLToPath(new URL("..", import.meta.url)));
const output = path.resolve(directory, "../../../work/electron-screenshots");
fs.mkdirSync(output, { recursive: true });
const env = { ...process.env };
delete env.ELECTRON_RUN_AS_NODE;
const app = await electron.launch({
  args: [directory, "--preview", "--smoke"],
  env,
  timeout: 30000,
});
const errors = [];
try {
  const page = await app.firstWindow();
  page.setDefaultTimeout(15000);
  page.on("pageerror", (e) => errors.push(e.message));
  await page.getByRole("tab", { name: "Overlay", exact: true }).click();
  await page.getByRole("switch", { name: "Enable overlays" }).waitFor();
  await page.evaluate(() => document.fonts.ready);
  await page.screenshot({
    path: path.join(output, "overlay-editor-inputs.png"),
  });
  assert.equal(await page.locator(".input-pedals .bar").count(), 3);
  await page.getByRole("checkbox", { name: "Clutch", exact: true }).uncheck();
  await page.waitForFunction(
    () => document.querySelectorAll(".input-pedals .bar").length === 2,
  );
  await page
    .getByRole("checkbox", { name: "Steering wheel", exact: true })
    .uncheck();
  assert.equal(await page.locator(".input-wheel").isVisible(), false);
  await page
    .getByRole("checkbox", { name: "Steering wheel", exact: true })
    .check();
  for (const [name, value] of [
    ["X", "100"],
    ["Y", "180"],
    ["Size", "125"],
    ["Opacity", "55"],
  ]) {
    await page.getByRole("spinbutton", { name, exact: true }).fill(value);
    await page.getByRole("spinbutton", { name, exact: true }).press("Tab");
  }
  await page.waitForFunction(
    async () =>
      (await window.sector.initializeOverlays()).config.widgets.inputs
        .opacity === 0.55,
  );
  const settings = await page.evaluate(() =>
    window.sector.initializeOverlays(),
  );
  assert.equal(settings.config.widgets.inputs.x, 100);
  assert.equal(settings.config.widgets.inputs.scale, 1.25);
  assert.equal(settings.config.widgets.inputs.showClutch, false);
  for (const name of [
    "Leaderboard",
    "Relative",
    "Weather",
    "Delta",
    "Laps",
    "Session",
  ]) {
    await page.getByRole("button", { name, exact: true }).click();
    await page.evaluate(() => document.fonts.ready);
    const widget = page.locator(".widget-stage .lmu-widget");
    await widget.screenshot({
      path: path.join(output, `widget-${name.toLowerCase()}.png`),
    });
    if (["Leaderboard", "Relative"].includes(name))
      assert.ok((await widget.locator(".manufacturer-logo").count()) > 0);
  }
  await page.getByRole("button", { name: "Inputs", exact: true }).click();
  await app.evaluate(() => {
    const o = global.sectorTest.overlays();
    const config = structuredClone(o.config);
    for (const w of Object.values(config.widgets)) w.enabled = true;
    o.save(config);
  });
  await page.getByRole("button", { name: "Edit layout", exact: true }).click();
  await page
    .getByRole("button", { name: "Lock layout", exact: true })
    .waitFor();
  await app.evaluate(async () => {
    const overlays = global.sectorTest.overlays();
    for (const win of overlays.windows.values()) {
      if (win.webContents.isLoading())
        await new Promise((r) => win.webContents.once("did-finish-load", r));
    }
  });
  const overlayPages = app.windows().filter((p) => p !== page);
  for (const popup of overlayPages) {
    popup.on("pageerror", (e) => errors.push(e.message));
    await popup.locator(".lmu-widget").waitFor();
    const current = await popup.evaluate(() =>
      window.sectorOverlay.initialize(),
    );
    await popup.evaluate(() => document.fonts.ready);
    const chrome = await popup.addStyleTag({
      content: ".layout-handle,.resize-handle{display:none}",
    });
    await popup.screenshot({
      path: path.join(output, `overlay-${current.id}.png`),
    });
    await chrome.evaluate((node) => node.remove());
    assert.equal(await popup.evaluate(() => typeof window.sector), "undefined");
    assert.equal(await popup.evaluate(() => typeof require), "undefined");
  }
  await app.evaluate(
    () =>
      new Promise((resolve, reject) => {
        const end = Date.now() + 5000;
        const poll = () => {
          if (global.sectorTest.overlays().windows.get("inputs")?.isVisible())
            return resolve();
          if (Date.now() > end)
            return reject(new Error("Edit window not visible"));
          setTimeout(poll, 30);
        };
        poll();
      }),
  );
  let inputs = await app.evaluate(() => {
    const o = global.sectorTest.overlays(),
      win = o.windows.get("inputs");
    const b = win.getBounds();
    return {
      bounds: b,
      editing: win.sectorEditing,
      visible: win.isVisible(),
      focusable: win.isFocusable(),
    };
  });
  assert.equal(inputs.editing, true);
  assert.equal(inputs.visible, true);
  assert.equal(inputs.bounds.width, 650);
  assert.equal(inputs.focusable, true);
  // Exercise actual BrowserWindow movement and persisted coordinates.
  await app.evaluate(() =>
    global.sectorTest.overlays().windows.get("inputs").setPosition(130, 210),
  );
  await page.waitForFunction(
    async () =>
      (await window.sector.initializeOverlays()).config.widgets.inputs.x ===
      130,
  );
  await page.getByRole("button", { name: "Lock layout", exact: true }).click();
  await page.waitForFunction(
    async () => !(await window.sector.initializeOverlays()).editing,
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.overlays().windows.size),
    0,
  );
  await page.getByRole("switch", { name: "Enable overlays" }).click();
  await page.waitForFunction(
    async () => (await window.sector.initializeOverlays()).config.enabled,
  );
  await app.evaluate(async () => {
    for (const win of global.sectorTest.overlays().windows.values())
      if (win.webContents.isLoading())
        await new Promise((r) => win.webContents.once("did-stop-loading", r));
  });
  inputs = await app.evaluate(() => {
    const w = global.sectorTest.overlays().windows.get("inputs");
    return {
      editing: w.sectorEditing,
      visible: w.isVisible(),
      focusable: w.isFocusable(),
    };
  });
  assert.deepEqual(inputs, {
    editing: false,
    visible: false,
    focusable: false,
  });
  await app.evaluate(() =>
    global.sectorTest.overlays().telemetry.publish(
      {
        connected: true,
        driving: true,
        foreground: true,
        session: 1,
        track: "Fixture",
        et: 100,
        remaining: 1000,
        length: 7000,
        cars: [],
        player: {
          id: 1,
          et: 100,
          lap: 1,
          lapStart: 0,
          speed: 80,
          gear: 3,
          throttle: 0.5,
          brake: 0,
          clutch: 0,
          steering: 0,
          fuel: 50,
        },
        history: [],
        laps: [],
      },
      "Live",
    ),
  );
  assert.equal(
    await app.evaluate(() =>
      global.sectorTest.overlays().windows.get("inputs").isVisible(),
    ),
    true,
  );
  await app.evaluate(() => {
    const o = global.sectorTest.overlays();
    o.telemetry.publish({ ...o.telemetry.frame, foreground: false }, "Live");
  });
  assert.equal(
    await app.evaluate(() =>
      global.sectorTest.overlays().windows.get("inputs").isVisible(),
    ),
    false,
  );
  await app.evaluate(() =>
    global.sectorTest.overlays().telemetry.publish(null, "Waiting for LMU"),
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  assert.equal(
    await app.evaluate(() =>
      Boolean(global.sectorTest.overlays().telemetry.child),
    ),
    false,
  );
  await app.evaluate(() => global.sectorTest.window().setSize(640, 600));
  await page.screenshot({
    path: path.join(output, "overlay-editor-compact.png"),
  });
  assert.equal(
    await page.evaluate(
      () => document.documentElement.scrollWidth > innerWidth,
    ),
    false,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PASS: all seven widgets, logos, settings, clutch removal, independent overlay windows, drag persistence, locking, foreground visibility and isolated preview.",
  );
} catch (error) {
  const page = app.windows()[0];
  if (page)
    await page
      .screenshot({ path: path.join(output, "overlay-failure.png") })
      .catch(() => {});
  console.error(errors);
  throw error;
} finally {
  await app.close().catch(() => {});
}
