import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import setup from "../main/onboarding.cjs";

function fixture(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "sector-onboarding-"));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  return root;
}

test("finds Steam games in external libraries, respects manifests and rejects missing executables", (t) => {
  const root = fixture(t);
  const steam = path.join(root, "Steam");
  const external = path.join(root, "Games on another drive");
  const game = path.join(external, "steamapps/common/LMU custom folder");
  fs.mkdirSync(path.join(steam, "steamapps"), { recursive: true });
  fs.mkdirSync(game, { recursive: true });
  fs.writeFileSync(
    path.join(steam, "steamapps/libraryfolders.vdf"),
    `"libraryfolders" { "1" { "path" ${JSON.stringify(external)} } }`,
  );
  fs.writeFileSync(
    path.join(external, "steamapps/appmanifest_2399420.acf"),
    '"AppState" { "installdir" "LMU custom folder" }',
  );
  assert.deepEqual(setup.steamCandidates([steam]), []);
  fs.writeFileSync(path.join(game, "Le Mans Ultimate.exe"), "fixture");
  assert.deepEqual(setup.steamCandidates([steam, external]), [game]);
  fs.writeFileSync(
    path.join(steam, "steamapps/libraryfolders.vdf"),
    `"LibraryFolders" { "1" ${JSON.stringify(external)} }`,
  );
  assert.deepEqual(setup.steamCandidates([steam]), [game]);
});

test("persists confirmed folder and completion separately, recovers missing or corrupt installations", (t) => {
  const root = fixture(t);
  const file = path.join(root, "preferences/onboarding.json");
  const binary = path.join(root, "Le Mans Ultimate.exe");
  assert.equal(setup.readSetup(file).completed, false);
  assert.throws(() => setup.writeSetup(file, { lmuPath: root }), /not found/);
  fs.writeFileSync(binary, "fixture");
  setup.writeSetup(file, { lmuPath: root, completed: false });
  assert.deepEqual(setup.readSetup(file), { lmuPath: root, completed: false });
  setup.writeSetup(file, { lmuPath: root, completed: true });
  assert.equal(setup.readSetup(file).completed, true);
  fs.unlinkSync(binary);
  assert.equal(setup.readSetup(file).completed, false);
  fs.writeFileSync(file, "broken json");
  assert.equal(setup.readSetup(file).completed, false);
});
