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
  await app.evaluate(() => global.sectorTest.window().showInactive());
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
  assert.equal(await page.getByLabel("Accent", { exact: true }).count(), 0);
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
    if (name === "Size") {
      await page.waitForFunction(
        async () =>
          (await window.sector.initializeOverlays()).config.widgets.inputs
            .scale === 1.25,
      );
      assert.equal(
        await page
          .getByRole("spinbutton", { name, exact: true })
          .evaluate((node) => node === document.activeElement),
        true,
      );
    }
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
    assert.equal(
      await page.getByLabel("Accent", { exact: true }).count(),
      name === "Session" ? 1 : 0,
    );
    await page.evaluate(() => document.fonts.ready);
    const widget = page.locator(".widget-stage .lmu-widget");
    if (name === "Session") {
      assert.doesNotMatch(
        await widget.innerText(),
        /LAP-TO-LAP SPREAD|penalty at/i,
      );
      assert.equal(
        await page
          .getByRole("checkbox", { name: "Column Spread", exact: true })
          .count(),
        0,
      );
      const lapsColumn = page.getByRole("checkbox", {
        name: "Column Laps",
        exact: true,
      });
      await lapsColumn.uncheck();
      assert.equal(await widget.getByText("LAPS", { exact: true }).count(), 0);
      assert.equal(
        await widget.getByText("PIT STOPS", { exact: true }).count(),
        1,
      );
      await lapsColumn.check();
    }
    if (name === "Laps")
      assert.doesNotMatch(await widget.innerText(), /Stint\s+\d/i);
    await widget.screenshot({
      path: path.join(output, `widget-${name.toLowerCase()}.png`),
    });
    if (["Leaderboard", "Relative"].includes(name))
      assert.ok((await widget.locator(".manufacturer-logo").count()) > 0);
  }
  await page.getByRole("button", { name: "Leaderboard", exact: true }).click();
  await page
    .getByRole("checkbox", { name: "Column Logo", exact: true })
    .uncheck();
  await page.waitForFunction(
    () =>
      document.querySelector(".widget-leaderboard > div").style.width ===
      "404px",
  );
  assert.equal(
    await page.locator('.widget-leaderboard [data-column="logo"]').count(),
    0,
  );
  await page
    .getByRole("checkbox", { name: "Column Logo", exact: true })
    .check();
  await page.getByRole("button", { name: "Laps", exact: true }).click();
  await page
    .getByRole("checkbox", { name: "Column Fuel", exact: true })
    .uncheck();
  await page.waitForFunction(
    () => document.querySelector(".widget-laps > div").style.width === "472px",
  );
  assert.equal(
    await page.locator('.widget-laps [data-column="fuel"]').count(),
    0,
  );
  await page
    .getByRole("checkbox", { name: "Column Fuel", exact: true })
    .check();
  await page.getByRole("button", { name: "Inputs", exact: true }).click();
  await app.evaluate(() => {
    const o = global.sectorTest.overlays();
    const config = structuredClone(o.config);
    for (const w of Object.values(config.widgets)) w.enabled = true;
    o.save(config);
  });
  const editWidth = (
    await page
      .getByRole("button", { name: "Edit layout", exact: true })
      .boundingBox()
  ).width;
  await page.getByRole("button", { name: "Edit layout", exact: true }).click();
  await page
    .getByRole("button", { name: "Lock layout", exact: true })
    .waitFor();
  assert.equal(
    (
      await page
        .getByRole("button", { name: "Lock layout", exact: true })
        .boundingBox()
    ).width,
    editWidth,
  );
  await app.evaluate(async () => {
    const overlays = global.sectorTest.overlays();
    for (const win of overlays.windows.values()) {
      if (win.webContents.isLoading())
        await new Promise((resolve, reject) => {
          const timeout = setTimeout(
            () => reject(new Error("Overlay load timeout")),
            15000,
          );
          win.webContents.once("did-stop-loading", () => {
            clearTimeout(timeout);
            resolve();
          });
        });
    }
  });
  const overlayPages = app.windows().filter((p) => p !== page);
  let inputPage;
  for (const popup of overlayPages) {
    popup.on("pageerror", (e) => errors.push(e.message));
    await popup.locator(".lmu-widget").waitFor();
    const current = await popup.evaluate(() =>
      window.sectorOverlay.initialize(),
    );
    if (current.id === "inputs") inputPage = popup;
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
  const resize = inputPage.getByRole("button", {
    name: "Resize widget",
    exact: true,
  });
  const handle = await resize.boundingBox();
  const start = {
    x: handle.x + handle.width / 2,
    y: handle.y + handle.height / 2,
  };
  await inputPage.mouse.move(start.x, start.y);
  await inputPage.mouse.down();
  await inputPage.mouse.move(start.x + 52, start.y, { steps: 5 });
  // Assert both native bounds and rendered scale change before releasing the mouse.
  await inputPage.waitForFunction(
    async () => (await window.sectorOverlay.initialize()).settings.scale > 1.3,
  );
  const liveWidth = await app.evaluate(
    () => global.sectorTest.overlays().windows.get("inputs").getBounds().width,
  );
  assert.ok(liveWidth > inputs.bounds.width);
  assert.ok(
    await inputPage
      .locator(".widget-scale")
      .evaluate(
        (node) => new DOMMatrix(getComputedStyle(node).transform).a > 1.3,
      ),
  );
  await inputPage.mouse.move(start.x + 78, start.y, { steps: 3 });
  await inputPage.mouse.up();
  await inputPage.waitForFunction(
    async () => (await window.sectorOverlay.initialize()).settings.scale > 1.35,
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.overlays().scaleTimer),
    null,
  );
  await inputPage.evaluate(() => window.sectorOverlay.scale(1.25));
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
  assert.equal(await page.locator(".overlay-connection").count(), 0);
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
        cars: [{ id: 1, player: true, class: "LMGT3", place: 1 }],
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
  // Session telemetry remains visible at the monitor; it does not require a physics callback.
  await app.evaluate(() => {
    const overlay = global.sectorTest.overlays();
    overlay.telemetry.publish(
      {
        connected: true,
        foreground: true,
        driving: false,
        cars: [{ id: 1, player: true, class: "LMGT3", place: 1 }],
        session: 1,
        track: "Spa",
      },
      "Live",
    );
  });
  assert.ok(
    await app.evaluate(() =>
      [...global.sectorTest.overlays().windows.values()].every((w) =>
        w.isVisible(),
      ),
    ),
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
