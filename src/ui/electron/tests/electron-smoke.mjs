import { _electron as electron } from "playwright";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawn } from "node:child_process";
import runtimeModule from "../main/runtime.cjs";

const directory = path.resolve(fileURLToPath(new URL("..", import.meta.url)));
const output = path.resolve(directory, "../../../work/electron-screenshots");
fs.mkdirSync(output, { recursive: true });
const env = { ...process.env };
delete env.ELECTRON_RUN_AS_NODE;
// This is read-only: verify the real discovery subprocess without launching a host.
assert.ok(Array.isArray(await runtimeModule.discoverGame()));
const app = await electron.launch({
  args: [directory, "--preview", "--smoke"],
  env,
  timeout: 30000,
});
const errors = [];
try {
  const page = await app.firstWindow();
  page.on("pageerror", (error) => errors.push(error.message));
  await page
    .getByRole("button", { name: "Change save shortcut, F6", exact: true })
    .waitFor();
  await page.waitForFunction(
    () => !document.querySelector(".key-button").disabled,
  );
  await page.evaluate(() => document.fonts.ready);
  const settle = () =>
    page.evaluate(() =>
      Promise.all(
        document
          .getAnimations()
          .map((animation) => animation.finished.catch(() => {})),
      ),
    );
  await settle();
  await page.screenshot({ path: path.join(output, "active-reset.png") });
  assert.equal(await page.evaluate(() => typeof window.require), "undefined");
  assert.equal(
    await page.evaluate(() => typeof window.sector.saveSettings),
    "function",
  );
  await page.getByRole("tab", { name: "Overlay", exact: true }).click();
  await page.getByText("Coming soon", { exact: true }).waitFor();
  await page.screenshot({ path: path.join(output, "overlay.png") });
  await page.getByRole("tab", { name: "Active Reset", exact: true }).click();
  await page
    .getByRole("button", { name: "Change save shortcut, F6", exact: true })
    .click();
  await page.getByRole("dialog").waitFor();
  await settle();
  await page.screenshot({ path: path.join(output, "capture.png") });
  await page.keyboard.press("F7");
  await page
    .getByText("That key is already assigned to the other action.")
    .waitFor();
  await page.keyboard.press("Control+G");
  await page
    .getByText("Use one key without Ctrl, Alt, Shift or Windows.")
    .waitFor();
  await page.keyboard.press("g");
  await page
    .getByRole("button", { name: "Change save shortcut, G", exact: true })
    .waitFor();
  await page
    .getByRole("button", { name: "Change restore shortcut, F7", exact: true })
    .click();
  await page.keyboard.press("Escape");
  await page.getByRole("dialog").waitFor({ state: "hidden" });
  assert.equal(
    await page
      .getByRole("button", { name: "Change restore shortcut, F7", exact: true })
      .count(),
    1,
  );
  await page.getByRole("button", { name: "Settings", exact: true }).click();
  await page.getByRole("dialog", { name: "Settings", exact: true }).waitFor();
  await settle();
  await page.screenshot({ path: path.join(output, "settings.png") });
  await page.getByRole("switch", { name: "Confirmation sounds" }).click();
  await page.waitForFunction(
    () =>
      document.querySelector('[role="switch"]').getAttribute("aria-checked") ===
      "false",
  );
  assert.equal(
    await page
      .getByRole("switch", { name: "Confirmation sounds" })
      .getAttribute("aria-checked"),
    "false",
  );
  assert.equal(
    await page
      .getByRole("button", { name: "Preview save", exact: true })
      .isDisabled(),
    true,
  );
  await page.getByRole("switch", { name: "Confirmation sounds" }).click();
  await page.waitForFunction(
    () =>
      document.querySelector('[role="switch"]').getAttribute("aria-checked") ===
      "true",
  );
  const sound = await page.evaluate(() => window.sector.previewSound("save"));
  assert.equal(sound.ok, true, sound.error);
  assert.ok(sound.source.startsWith("data:audio/wav;base64,"));
  const invalid = await page.evaluate(() =>
    window.sector.saveSettings({
      save_key: "F6",
      restore_key: "F6",
      sound: true,
    }),
  );
  assert.equal(invalid.ok, false);
  await page.getByRole("switch", { name: "Reduce motion" }).click();
  await page.waitForFunction(
    () => document.documentElement.dataset.reduceMotion === "true",
  );
  await page.getByRole("button", { name: "Done", exact: true }).click();
  await page.getByRole("dialog").waitFor({ state: "hidden" });
  await page.getByRole("button", { name: "Settings", exact: true }).click();
  assert.equal(
    await page
      .getByRole("switch", { name: "Reduce motion" })
      .getAttribute("aria-checked"),
    "true",
  );
  await page.keyboard.press("Escape");
  await page.getByRole("dialog").waitFor({ state: "hidden" });
  await app.evaluate(() =>
    global.sectorTest.publish({
      phase: "active",
      checkpoint: true,
      saved_speed_kmh: 80.3,
      saved_at: 1750000000,
      restore_count: 7,
      message: "Checkpoint applied.",
    }),
  );
  await page.getByText("Checkpoint applied.", { exact: true }).waitFor();
  assert.equal(
    await page.locator(".checkpoint-title").innerText(),
    "Checkpoint ready",
  );
  await page.screenshot({ path: path.join(output, "checkpoint.png") });
  await app.evaluate(() =>
    global.sectorTest.publish({
      phase: "paused",
      checkpoint: false,
      message: "Vehicle changed. Waiting for driving.",
    }),
  );
  await page.getByText("Vehicle changed. Waiting for driving.").waitFor();
  assert.equal(
    await page.locator(".checkpoint-title").innerText(),
    "No checkpoint",
  );
  await page.getByRole("button", { name: "Close window", exact: true }).click();
  await page.getByRole("heading", { name: "Keep Sector running?" }).waitFor();
  await settle();
  await page.screenshot({ path: path.join(output, "close.png") });
  await page.keyboard.press("Tab");
  assert.equal(
    await page.evaluate(() =>
      Boolean(document.activeElement.closest('[role="dialog"]')),
    ),
    true,
  );
  await page
    .getByRole("button", { name: "Run in background", exact: true })
    .click();
  await page.getByRole("dialog").waitFor({ state: "hidden" });
  assert.equal(
    await app.evaluate(() => global.sectorTest.window().isVisible()),
    false,
  );
  const reopened = spawn(
    "powershell.exe",
    [
      "-NoProfile",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      path.resolve(directory, "../../../scripts/Start-ResetGui.ps1"),
      "-Preview",
    ],
    {
      env,
      windowsHide: true,
      stdio: "inherit",
    },
  );
  const secondExit = await new Promise((resolve, reject) => {
    const timeout = setTimeout(
      () => reject(new Error("Second instance did not exit.")),
      30000,
    );
    reopened.once("error", reject);
    reopened.once("exit", (code) => {
      clearTimeout(timeout);
      resolve(code);
    });
  });
  assert.equal(secondExit, 0);
  await app.evaluate(
    () =>
      new Promise((resolve, reject) => {
        const deadline = Date.now() + 10000;
        const check = () => {
          if (global.sectorTest.window().isVisible()) return resolve();
          if (Date.now() > deadline)
            return reject(
              new Error("Launcher did not reveal the existing window."),
            );
          setTimeout(check, 50);
        };
        check();
      }),
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.window().isVisible()),
    true,
  );
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  await page.emulateMedia({ reducedMotion: "reduce" });
  await app.evaluate(() => global.sectorTest.window().setSize(640, 600));
  await page
    .getByRole("button", { name: "Change save shortcut, G", exact: true })
    .waitFor();
  await settle();
  const overflow = await page.evaluate(
    () => document.documentElement.scrollWidth > innerWidth,
  );
  assert.equal(overflow, false);
  await page.screenshot({ path: path.join(output, "compact.png") });
  await page.getByRole("button", { name: "Close window", exact: true }).click();
  await page.getByRole("dialog").waitFor();
  assert.equal(
    await page
      .locator(".modal")
      .evaluate((node) => getComputedStyle(node).transform),
    "none",
  );
  assert.deepEqual(errors, []);
  const closed = app.waitForEvent("close", { timeout: 10000 });
  await page
    .getByRole("button", { name: "Quit Sector", exact: true })
    .click({ noWaitAfter: true })
    .catch((error) => {
      if (!page.isClosed()) throw error;
    });
  await closed;
  console.log(
    "PASS: Electron isolation, tabs, key capture, validation, sound, acknowledgements, focus, background, compact layout, reduced motion and quit.",
  );
} finally {
  await app.close().catch(() => {});
}
