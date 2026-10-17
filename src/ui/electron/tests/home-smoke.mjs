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
});
const errors = [];
try {
  const page = await app.firstWindow();
  page.setDefaultTimeout(15000);
  page.on("pageerror", (e) => errors.push(e.message));
  await page.getByRole("heading", { name: "Home.", exact: true }).waitFor();
  await page.getByText("2 of 7 selected, Locked", { exact: true }).waitFor();
  await app.evaluate(() => global.sectorTest.window().showInactive());
  assert.equal(
    await page
      .getByRole("button", { name: "Launch LMU", exact: true })
      .isDisabled(),
    true,
  );
  assert.equal(
    (await page.evaluate(() => window.sector.launchGame())).ok,
    false,
  );
  await page
    .getByRole("button", { name: "Change save shortcut, F6", exact: true })
    .click();
  await page.keyboard.press("G");
  await page.getByRole("dialog").waitFor({ state: "hidden" });
  await page
    .getByRole("button", { name: "Change save shortcut, G", exact: true })
    .waitFor();
  await page
    .getByRole("button", { name: "Open Active Reset", exact: true })
    .click();
  await page
    .getByRole("heading", { name: "Active Reset.", exact: true })
    .waitFor();
  await page
    .getByRole("button", { name: "Change save shortcut, G", exact: true })
    .waitFor();
  await page.getByRole("tab", { name: "Home", exact: true }).click();
  await page
    .getByRole("switch", { name: "Enable overlays", exact: true })
    .click();
  await page.waitForFunction(
    async () => (await window.sector.initializeOverlays()).config.enabled,
  );
  await page.getByRole("button", { name: "Edit layout", exact: true }).click();
  await page
    .getByRole("button", { name: "Lock layout", exact: true })
    .waitFor();
  await page.getByRole("button", { name: "Lock layout", exact: true }).click();
  await page.getByRole("tab", { name: "Home", exact: true }).click();
  await page
    .getByRole("switch", { name: "Enable overlays", exact: true })
    .click();
  await page.waitForFunction(
    async () => !(await window.sector.initializeOverlays()).config.enabled,
  );
  await app.evaluate(() => {
    global.sectorTest.publish({
      phase: "active",
      checkpoint: true,
      restore_count: 4,
    });
    global.sectorTest.overlays().telemetry.publish(
      {
        connected: true,
        foreground: false,
        session: 1,
        track: "Spa-Francorchamps",
        cars: [
          {
            id: 1,
            player: true,
            vehicle: "Mercedes AMG GT3",
            class: "LMGT3",
            laps: 17,
            best: 138.412,
            place: 1,
          },
        ],
        player: { id: 1 },
      },
      "Live",
    );
  });
  await page.getByText("Checkpoint saved", { exact: true }).waitFor();
  await page.getByText("Spa-Francorchamps", { exact: true }).waitFor();
  await page.getByText("2:18.412", { exact: true }).waitFor();
  assert.equal(
    await app.evaluate(() => global.sectorTest.overlays().windows.size),
    0,
  );
  const checkpoint = await page.locator(".home-checkpoint").boundingBox();
  const shortcuts = await page.locator(".home-keys").boundingBox();
  assert.ok(checkpoint.y >= shortcuts.y + shortcuts.height);
  await page.evaluate(() => document.fonts.ready);
  await page.evaluate(() =>
    Promise.all(
      document.getAnimations().map((a) => a.finished.catch(() => {})),
    ),
  );
  await page.screenshot({ path: path.join(output, "home-session.png") });
  await app.evaluate(() => global.sectorTest.window().setSize(640, 600));
  await page.evaluate(() =>
    Promise.all(
      document.getAnimations().map((a) => a.finished.catch(() => {})),
    ),
  );
  assert.equal(
    await page.evaluate(
      () => document.documentElement.scrollWidth > innerWidth,
    ),
    false,
  );
  await page.screenshot({ path: path.join(output, "home-compact.png") });
  await app.evaluate(() => {
    global.sectorTest.publish({ phase: "idle", checkpoint: false });
    global.sectorTest.overlays().telemetry.publish(null, "Waiting for LMU");
  });
  await page.getByText("Ready when you are", { exact: true }).waitFor();
  await page.getByText("No checkpoint saved", { exact: true }).waitFor();
  assert.equal(
    await page.getByText("Spa-Francorchamps", { exact: true }).count(),
    0,
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PASS Home: live session, checkpoint, shared shortcuts, overlay controls, layout preview, compact window and preview isolation.",
  );
} finally {
  await app.close();
}
