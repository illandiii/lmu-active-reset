import { _electron as electron } from "playwright";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";

const ui = process.cwd();
const root = path.resolve(ui, "../../..");
const fixture = fs.mkdtempSync(path.join(root, "work/onboarding-smoke-"));
const game = path.join(fixture, "Le Mans Ultimate");
const setupFile = path.join(fixture, "onboarding.json");
fs.mkdirSync(game);
fs.writeFileSync(
  path.join(game, "Le Mans Ultimate.exe"),
  "test fixture, never executed",
);
const packaged = process.argv.includes("--packaged");
const version = JSON.parse(fs.readFileSync("package.json", "utf8")).version;
const env = {
  ...process.env,
  SECTOR_TEST_LMU: game,
  SECTOR_TEST_SETUP_FILE: setupFile,
};
delete env.ELECTRON_RUN_AS_NODE;
let app;
const errors = [];

async function launch(detect = true) {
  app = await electron.launch({
    ...(packaged
      ? {
          executablePath: path.join(
            root,
            `releases/Sector-${version}-win-x64/Sector.exe`,
          ),
        }
      : {}),
    args: [
      ...(packaged ? [] : [ui]),
      "--preview",
      "--smoke",
      "--onboarding-smoke",
    ],
    env: { ...env, SECTOR_TEST_LMU: detect ? game : "" },
    timeout: 30000,
  });
  const page = await app.firstWindow();
  await page.emulateMedia({ reducedMotion: "no-preference" });
  page.setDefaultTimeout(15000);
  page.on("pageerror", (error) => errors.push(error.message));
  // The modal makes the underlying controls inaccessible until setup finishes.
  await page.waitForFunction(() => {
    const button = document.querySelector(".home-key");
    return button && !button.disabled;
  });
  // Keep Chromium's compositor active while checking dialogs and screenshots.
  await app.evaluate(() => global.sectorTest.window().showInactive());
  return page;
}

try {
  let page = await launch();
  await page.getByText(game, { exact: true }).waitFor();
  assert.equal(
    await page
      .locator(".onboarding")
      .evaluate((node) => getComputedStyle(node).animationName),
    "onboarding-enter",
  );
  await page.keyboard.press("Escape");
  assert.equal(
    await page
      .getByRole("dialog", { name: "Connect Le Mans Ultimate" })
      .isVisible(),
    true,
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  await page.screenshot({
    path: path.join(root, "work/electron-screenshots/onboarding-folder.png"),
  });
  await page
    .getByRole("button", { name: "Confirm folder", exact: true })
    .click();
  await page.getByRole("dialog", { name: "Ready for a quick tour?" }).waitFor();
  assert.equal(
    await page
      .locator(".onboarding-step")
      .evaluate((node) => getComputedStyle(node).animationName),
    "onboarding-step-enter",
  );
  assert.equal(JSON.parse(fs.readFileSync(setupFile)).completed, false);
  await page.screenshot({
    path: path.join(root, "work/electron-screenshots/onboarding-tour.png"),
  });
  await page.getByRole("button", { name: "Start guide", exact: true }).click();
  await page.locator(".tour-popup").waitFor();
  assert.equal(
    await page
      .locator(".tour-popup")
      .evaluate((node) => getComputedStyle(node).animationName),
    "tour-enter-a",
  );
  await page.getByRole("button", { name: "Next", exact: true }).click();
  assert.equal(
    await page
      .locator(".tour-popup")
      .evaluate((node) => getComputedStyle(node).animationName),
    "tour-enter-b",
  );
  await page.emulateMedia({ reducedMotion: "reduce" });
  assert.equal(
    await page
      .locator(".tour-popup")
      .evaluate((node) => getComputedStyle(node).animationName),
    "none",
  );
  await page.emulateMedia({ reducedMotion: "no-preference" });
  await page.evaluate(() => {
    document.documentElement.dataset.reduceMotion = "true";
  });
  assert.equal(
    await page
      .locator(".tour-popup")
      .evaluate((node) => getComputedStyle(node).animationName),
    "none",
  );
  assert.equal(JSON.parse(fs.readFileSync(setupFile)).completed, true);
  await page.getByRole("button", { name: "Skip", exact: true }).click();
  await app.close();

  page = await launch();
  assert.equal(await page.getByRole("dialog").count(), 0);
  assert.equal(await page.locator(".tour-popup").count(), 0);
  assert.equal(
    (await page.evaluate(() => window.sector.initialize())).onboarding.required,
    false,
  );
  await app.close();

  fs.unlinkSync(setupFile);
  page = await launch(false);
  await page.getByText("Choose your LMU folder", { exact: true }).waitFor();
  assert.equal(
    await page.getByRole("button", { name: "Confirm folder" }).isDisabled(),
    true,
  );
  await app.evaluate(({ dialog }) => {
    dialog.showOpenDialog = async () => ({ canceled: true, filePaths: [] });
  });
  await page.getByRole("button", { name: "Browse…" }).click();
  await page.getByRole("button", { name: "Browse…" }).waitFor();
  await app.evaluate(({ dialog }, folder) => {
    dialog.showOpenDialog = async () => ({
      canceled: false,
      filePaths: [folder],
    });
  }, fixture);
  await page.getByRole("button", { name: "Browse…" }).click();
  await page.getByRole("alert").waitFor();
  assert.match(await page.getByRole("alert").innerText(), /not found/);
  await app.evaluate(({ dialog }, folder) => {
    dialog.showOpenDialog = async () => ({
      canceled: false,
      filePaths: [folder],
    });
  }, game);
  await page.getByRole("button", { name: "Browse…" }).click();
  await page.getByText(game, { exact: true }).waitFor();
  await page.getByRole("button", { name: "Confirm folder" }).click();
  await page.getByRole("button", { name: "Not now", exact: true }).click();
  await page.getByRole("dialog").waitFor({ state: "hidden" });
  assert.equal(await page.locator(".tour-popup").count(), 0);
  assert.equal(JSON.parse(fs.readFileSync(setupFile)).completed, true);
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PASS onboarding: detected/manual folder, invalid/canceled picker, guide opt-in/skip, persistent first-run completion and preview isolation" +
      (packaged ? " (packaged EXE)" : ""),
  );
} finally {
  if (app) await app.close().catch(() => {});
  fs.rmSync(fixture, { recursive: true, force: true });
}
