import { _electron as electron } from "playwright";
import assert from "node:assert/strict";
import path from "node:path";
import fs from "node:fs";
import { fileURLToPath } from "node:url";

const directory = path.resolve(fileURLToPath(new URL("..", import.meta.url)));
const history = JSON.parse(
  fs.readFileSync(path.join(directory, "shared/changelog.json"), "utf8"),
);
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
  await page.getByText("2 of 10 selected, Locked", { exact: true }).waitFor();
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
  assert.equal(
    await page
      .getByRole("button", { name: "Save checkpoint", exact: true })
      .isDisabled(),
    true,
  );
  assert.equal(
    await page
      .getByRole("button", { name: "Restore checkpoint", exact: true })
      .isDisabled(),
    true,
  );
  await page
    .getByRole("button", { name: "Open Active Reset", exact: true })
    .click();
  await page
    .getByRole("heading", { name: "Active Reset.", exact: true })
    .waitFor();
  await page
    .getByRole("button", { name: "Change save shortcut, F6", exact: true })
    .click();
  await page.keyboard.press("G");
  await page.getByRole("dialog").waitFor({ state: "hidden" });
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
  const saveButton = page.getByRole("button", {
    name: "Save checkpoint",
    exact: true,
  });
  const restoreButton = page.getByRole("button", {
    name: "Restore checkpoint",
    exact: true,
  });
  await saveButton.click();
  await page
    .getByRole("alert")
    .getByText("Reset is unavailable in this session.", { exact: true })
    .waitFor();
  assert.equal(await page.getByRole("dialog").count(), 0);
  await restoreButton.click();
  await page
    .getByRole("alert")
    .getByText("Reset is unavailable in this session.", { exact: true })
    .waitFor();
  assert.equal(await page.getByRole("dialog").count(), 0);
  // Hold the overlay save open to catch unrelated buttons being disabled/replaced.
  await app.evaluate(() => {
    const overlays = global.sectorTest.overlays();
    const save = overlays.save.bind(overlays);
    overlays.save = async (config) => {
      await new Promise((resolve) => setTimeout(resolve, 350));
      return save(config);
    };
  });
  await page.evaluate(() => {
    window.originalHomeButtons = [
      ...document.querySelectorAll(
        ".home-key, .home-open-reset, .home-overlay-actions button",
      ),
    ];
    window.buttonChanges = [];
    window.buttonObserver = new MutationObserver((changes) =>
      window.buttonChanges.push(...changes.map((c) => c.attributeName)),
    );
    window.originalHomeButtons.forEach((button) =>
      window.buttonObserver.observe(button, {
        attributes: true,
        attributeFilter: ["disabled"],
      }),
    );
  });
  await page
    .getByRole("switch", { name: "Enable overlays", exact: true })
    .click();
  await page.waitForFunction(
    () =>
      document
        .querySelector(".home-overlay-toggle")
        .getAttribute("aria-busy") === "false",
  );
  assert.deepEqual(await page.evaluate(() => window.buttonChanges), []);
  assert.equal(
    await page.evaluate(() =>
      window.originalHomeButtons.every((button) => button.isConnected),
    ),
    true,
  );
  await page.evaluate(() => window.buttonObserver.disconnect());
  await page
    .getByRole("switch", { name: "Enable overlays", exact: true })
    .click();
  await page.waitForFunction(
    () =>
      document
        .querySelector(".home-overlay-toggle")
        .getAttribute("aria-busy") === "false",
  );
  await page.getByRole("button", { name: "Help", exact: true }).click();
  await page.getByRole("separator").waitFor();
  await page.getByRole("menuitem", { name: "Changelog", exact: true }).click();
  const changelog = page.getByRole("dialog", {
    name: "Changelog",
    exact: true,
  });
  await changelog
    .getByRole("heading", {
      name: new RegExp(
        `${history[0].version.replaceAll(".", "\\.")}\\s*Latest`,
      ),
    })
    .waitFor();
  assert.equal(
    await changelog.locator(".changelog-list section").count(),
    history.length,
  );
  await changelog
    .getByRole("heading", { name: "0.1.0", exact: true })
    .scrollIntoViewIfNeeded();
  await changelog.locator(".changelog-list").evaluate((el) => {
    el.scrollTop = 0;
  });
  await page.screenshot({ path: path.join(output, "changelog.png") });
  await changelog.getByRole("button", { name: "Done", exact: true }).click();
  await changelog.waitFor({ state: "hidden" });
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
    "PASS Home: reset actions, Active Reset shortcuts, independent overlay controls, changelog, live session, layout and preview isolation.",
  );
} finally {
  await app.close();
}
