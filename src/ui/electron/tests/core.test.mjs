import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { EventEmitter } from "node:events";
import settingsModule from "../main/settings.cjs";
import runtimeModule from "../main/runtime.cjs";
import { keyFromEvent } from "../renderer/keys.js";
import { statusView } from "../renderer/status.js";
const { defaults, keys, validateSettings, readSettings, writeSettings } =
  settingsModule;
const { Runtime } = runtimeModule;

function fixture(t, options = {}) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "sector-electron-"));
  const spawned = [];
  let clock = 1000;
  const runtime = new Runtime({
    root,
    settingsFile: path.join(root, "settings.json"),
    getSettings: () => defaults,
    discover: async () => [{ pid: 42, identity: "42:1" }],
    now: () => clock,
    spawnChild: (...args) => {
      const child = new EventEmitter();
      spawned.push({ args, child });
      return child;
    },
    ...options,
  });
  t.after(() => {
    runtime.dispose();
    fs.rmSync(root, { recursive: true, force: true });
  });
  return {
    root,
    runtime,
    spawned,
    advance: () => {
      clock += 2000;
    },
    report: (value) =>
      fs.writeFileSync(
        path.join(runtime.session, "status.json"),
        JSON.stringify({ schema_version: 1, sequence: 1, ...value }),
      ),
  };
}

test("migrates existing BOM preferences and atomically persists canonical keys", (t) => {
  const { root } = fixture(t);
  const file = path.join(root, "settings.json");
  assert.deepEqual(readSettings(file), defaults);
  fs.writeFileSync(
    file,
    "\uFEFF" +
      JSON.stringify({ save_key: "g", restore_key: "f7", sound: false }),
  );
  assert.deepEqual(readSettings(file), {
    save_key: "G",
    restore_key: "F7",
    sound: false,
    reduce_motion: false,
  });
  writeSettings(file, {
    save_key: "NumPad3",
    restore_key: "Space",
    sound: true,
    reduce_motion: true,
  });
  assert.equal(readSettings(file).reduce_motion, true);
  assert.equal(readSettings(file).save_key, "NumPad3");
  assert.equal(
    fs.readdirSync(root).filter((name) => name.endsWith(".tmp")).length,
    0,
  );
  assert.throws(() => writeSettings(file, { ...defaults, restore_key: "F6" }));
  assert.equal(readSettings(file).restore_key, "Space");
  fs.writeFileSync(file, "{broken");
  assert.throws(() => readSettings(file), /Restore defaults/);
});

test("rejects unsupported keys, duplicate assignments and nonboolean sound", () => {
  for (const value of [
    null,
    { ...defaults, save_key: "Escape" },
    { ...defaults, restore_key: "f6" },
    { ...defaults, sound: "true" },
    { ...defaults, reduce_motion: "false" },
    { ...defaults, save_key: "F6 & calc.exe" },
  ])
    assert.throws(() => validateSettings(value));
  assert.equal(
    validateSettings({ ...defaults, extraneous: "ignored" }).extraneous,
    undefined,
  );
});

test("keyboard mapping respects layout letters, numpad, modifiers and repeats", () => {
  const key = (code, name = code, extra = {}) =>
    keyFromEvent({ code, key: name, ...extra }, keys);
  assert.equal(key("KeyQ", "a"), "A");
  assert.equal(key("Digit4", "4"), "D4");
  assert.equal(key("Numpad4", "4"), "NumPad4");
  assert.equal(key("NumpadEnter", "Enter"), "Enter");
  assert.equal(key("Space", " "), "Space");
  assert.equal(key("F10", "F10", { ctrlKey: true }), null);
  assert.equal(key("KeyG", "g", { repeat: true }), null);
  assert.equal(key("Escape"), null);
});

test("view shows only acknowledged checkpoints and sanitizes malformed values", () => {
  const noCheckpoint = statusView({
    checkpoint: false,
    saved_speed_kmh: 200,
    restore_count: 20,
  });
  assert.equal(noCheckpoint.speed, "-");
  assert.equal(noCheckpoint.count, 0);
  const saved = statusView({
    phase: "active",
    checkpoint: true,
    saved_speed_kmh: 80.3,
    restore_count: 3,
  });
  assert.equal(saved.speed, "80");
  assert.equal(saved.count, 3);
  assert.equal(
    statusView({ checkpoint: true, saved_speed_kmh: NaN }).speed,
    "-",
  );
});

test("waits for LMU, launches exactly once per process identity and passes safe argv", async (t) => {
  let games = [];
  const { runtime, spawned, advance } = fixture(t, {
    discover: async () => games,
  });
  await runtime.tick();
  assert.equal(spawned.length, 0);
  games = [{ pid: 42, identity: "42:1" }];
  advance();
  await runtime.tick();
  assert.equal(spawned.length, 1);
  const [python, args, options] = spawned[0].args;
  assert.ok(python);
  assert.ok(args.includes("--gui-session"));
  assert.ok(args.includes("--allow-writes"));
  assert.equal(options.windowsHide, true);
  assert.equal(options.shell, undefined);
  spawned[0].child.emit("close", 1);
  advance();
  await runtime.tick();
  assert.equal(spawned.length, 1);
  games = [{ pid: 42, identity: "42:2" }];
  advance();
  await runtime.tick();
  assert.equal(spawned.length, 2);
});

test("reports native acknowledgement, retains failed-save checkpoint and invalidates on detach", async (t) => {
  const { runtime, report, spawned } = fixture(t);
  await runtime.tick();
  report({
    checkpoint: true,
    saved_speed_kmh: 80,
    restore_count: 2,
    phase: "active",
  });
  await runtime.tick();
  assert.equal(runtime.state.saved_speed_kmh, 80);
  report({
    sequence: 2,
    checkpoint: true,
    saved_speed_kmh: 80,
    restore_count: 2,
    message: "Command rejected",
  });
  await runtime.tick();
  assert.equal(runtime.state.checkpoint, true);
  spawned[0].child.emit("close", 0);
  assert.equal(runtime.state.checkpoint, false);
  assert.equal(runtime.state.restore_count, 0);
});

test("quit writes cooperative stop and waits for child close while heartbeat continues", async (t) => {
  const { runtime, spawned, advance } = fixture(t);
  await runtime.tick();
  let done = false;
  const stop = runtime.stop().then(() => {
    done = true;
  });
  assert.ok(fs.existsSync(path.join(runtime.session, "stop")));
  assert.equal(done, false);
  fs.rmSync(path.join(runtime.session, "heartbeat"));
  advance();
  await runtime.tick();
  assert.ok(fs.existsSync(path.join(runtime.session, "heartbeat")));
  spawned[0].child.emit("close", 0);
  await stop;
  assert.equal(done, true);
});

test("quit during asynchronous discovery cannot launch a host", async (t) => {
  let release;
  const { runtime, spawned } = fixture(t, {
    discover: () =>
      new Promise((resolve) => {
        release = resolve;
      }),
  });
  const tick = runtime.tick();
  await runtime.stop();
  release([{ pid: 42, identity: "42:1" }]);
  await tick;
  assert.equal(spawned.length, 0);
});

test("overlapping ticks cannot launch competing listeners", async (t) => {
  let release;
  const { runtime, spawned } = fixture(t, {
    discover: () =>
      new Promise((resolve) => {
        release = resolve;
      }),
  });
  const first = runtime.tick();
  await runtime.tick();
  release([{ pid: 42, identity: "42:1" }]);
  await first;
  assert.equal(spawned.length, 1);
});
