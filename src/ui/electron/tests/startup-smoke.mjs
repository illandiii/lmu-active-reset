import { _electron as electron } from "playwright";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import settings from "../main/settings.cjs";

const ui = process.cwd();
const root = path.resolve(ui, "../../..");
const previewSettings = path.join(
  root,
  "work/electron-preview/LMUActiveReset/settings.json",
);
const original = fs.existsSync(previewSettings)
  ? fs.readFileSync(previewSettings)
  : null;
const output = path.join(root, "work/electron-screenshots");
fs.mkdirSync(path.dirname(previewSettings), { recursive: true });
fs.mkdirSync(output, { recursive: true });
const env = { ...process.env };
delete env.ELECTRON_RUN_AS_NODE;
let app;
const errors = [];
async function launch(reduce = false) {
  fs.writeFileSync(
    previewSettings,
    JSON.stringify({ ...settings.defaults, reduce_motion: reduce }),
  );
  app = await electron.launch({ args: [ui, "--preview", "--smoke"], env });
  const page = await app.firstWindow();
  page.on("pageerror", (error) => errors.push(error.message));
  await page.locator("#sector-splash").waitFor();
  await app.evaluate(() => global.sectorTest.window().showInactive());
  return page;
}
try {
  let page = await launch();
  await page.emulateMedia({ reducedMotion: "no-preference" });
  assert.equal(await page.locator(".word-stroke").count(), 8);
  assert.equal(
    await page
      .locator(".splash-logo")
      .evaluate((el) => el.complete && el.naturalWidth > 0),
    true,
  );
  assert.equal(
    await page.getByRole("tab", { name: "Home", exact: true }).count(),
    0,
  );
  await page.waitForFunction(() => {
    const el = document.querySelector(".word-stroke");
    const offset = Number.parseFloat(getComputedStyle(el).strokeDashoffset);
    return offset > 0 && offset < 0.9;
  });
  await page.screenshot({ path: path.join(output, "startup-drawing.png") });
  await page.waitForFunction(
    () =>
      Number(getComputedStyle(document.querySelector(".word-dot")).opacity) >
      0.95,
  );
  await page.screenshot({ path: path.join(output, "startup-complete.png") });
  await page.locator("#sector-splash").waitFor({ state: "detached" });
  await page.getByRole("tab", { name: "Home", exact: true }).waitFor();
  await page.evaluate(() => window.sector.background());
  assert.equal(
    await app.evaluate(() => global.sectorTest.window().isVisible()),
    false,
  );
  await app.evaluate(() => global.sectorTest.show());
  assert.equal(await page.locator("#sector-splash").count(), 0);
  await page.reload();
  await page.getByRole("tab", { name: "Home", exact: true }).waitFor();
  assert.equal(await page.locator("#sector-splash").count(), 0);
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  await app.close();

  for (const skip of ["keyboard", "pointer"]) {
    page = await launch();
    const start = Date.now();
    if (skip === "keyboard") await page.keyboard.press("Enter");
    else await page.locator(".splash-card").click();
    await page.locator("#sector-splash").waitFor({ state: "detached" });
    assert.ok(Date.now() - start < 2500);
    assert.equal(await page.getByRole("dialog").count(), 0);
    await app.close();
  }

  page = await launch(true);
  const start = Date.now();
  assert.equal(
    await page
      .locator(".word-stroke")
      .first()
      .evaluate((el) => getComputedStyle(el).display),
    "none",
  );
  await page.locator("#sector-splash").waitFor({ state: "detached" });
  assert.ok(Date.now() - start < 2500);
  assert.deepEqual(errors, []);
  console.log(
    "PASS cold-start drawing, automatic exit, skip input, background/reload suppression and reduced motion.",
  );
} finally {
  await app?.close().catch(() => {});
  if (original) fs.writeFileSync(previewSettings, original);
  else fs.rmSync(previewSettings, { force: true });
}
