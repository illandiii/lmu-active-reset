import { _electron as electron } from "playwright";
import assert from "node:assert/strict";
import path from "node:path";
import fs from "node:fs";
import { execFileSync } from "node:child_process";
const root = path.resolve("../../..");
const version = JSON.parse(fs.readFileSync("package.json", "utf8")).version;
const release = path.join(root, `releases/Sector-${version}-win-x64`);
const payload = path.join(release, "resources/runtime");
const env = { ...process.env, PYTHONDONTWRITEBYTECODE: "1" };
delete env.ELECTRON_RUN_AS_NODE;
delete env.SECTOR_PYTHON;
const python = path.join(payload, "python/python.exe");
const nativeFrame = JSON.parse(
  execFileSync(path.join(payload, "bin/overlay-reader.exe"), ["--test-frame"], {
    encoding: "utf8",
    windowsHide: true,
  }),
);
assert.deepEqual(nativeFrame.player.wheels[0].temperatures, [98, 93, 88]);
execFileSync(
  python,
  [
    path.join(payload, "scripts/sector.py"),
    "runtime.start_reset_hotkeys",
    "--help",
  ],
  { cwd: process.env.TEMP, env },
);
execFileSync(
  python,
  [
    "-c",
    `import sys; sys.path.insert(0,${JSON.stringify(path.join(payload, "src/python"))}); import sector.runtime.run_physical_reset_host_25661166; import sector.observers.audit_snapshot_25661166; import sector.desktop.reset_feedback`,
  ],
  { cwd: process.env.TEMP, env },
);
const app = await electron.launch({
  executablePath: path.join(release, "Sector.exe"),
  args: ["--preview", "--smoke"],
  env,
  timeout: 30000,
});
try {
  const page = await app.firstWindow();
  await page
    .getByRole("button", { name: "Change save shortcut, F6", exact: true })
    .waitFor();
  await app.evaluate(() => global.sectorTest.window().showInactive());
  assert.equal(await app.evaluate(({ app }) => app.getVersion()), version);
  assert.equal(await app.evaluate(({ app }) => app.isPackaged), true);
  assert.equal(
    await app.evaluate(() => global.sectorTest.hasListener()),
    false,
  );
  const shippedPython = await app.evaluate(() => process.env.SECTOR_PYTHON);
  assert.ok(shippedPython.endsWith("python\\python.exe"));
  await page.getByRole("tab", { name: "Overlay", exact: true }).click();
  await page
    .getByRole("switch", { name: "Enable overlays", exact: true })
    .waitFor();
  for (const name of ["Brake lock", "Tyres", "Stint planner", "Hybrid"]) {
    await page.getByRole("button", { name, exact: true }).click();
    await page.locator(".widget-stage .lmu-widget").waitFor();
    assert.doesNotMatch(
      await page.locator(".widget-stage").innerText(),
      /undefined|NaN|{{/,
    );
    if (name === "Tyres") {
      await page
        .locator(".widget-tyres")
        .getByText("~flat spot", { exact: true })
        .waitFor();
      assert.equal(
        await page
          .getByRole("spinbutton", {
            name: "Flat spot sensitivity",
            exact: true,
          })
          .inputValue(),
        "50",
      );
    }
  }
  fs.mkdirSync(path.join(root, "work/release-tests"), { recursive: true });
  await page.screenshot({
    path: path.join(root, "work/release-tests/sector-release.png"),
  });
  console.log(
    "PASS packaged Sector.exe, isolated preview, bundled Python imports, UI and overlays",
  );
} finally {
  await app.close();
}
