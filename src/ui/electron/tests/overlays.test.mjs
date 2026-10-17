import { test } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import config from "../shared/overlay-config.cjs";
import model from "../shared/overlay-model.cjs";
const raw = (patch = {}) => ({
  schema: 1,
  connected: true,
  driving: true,
  foreground: true,
  track: "Spa",
  session: 1,
  et: 100,
  length: 7000,
  remaining: 1000,
  air: 17,
  trackTemp: 24,
  rain: 0,
  cloud: 0.4,
  wetness: 0,
  limitStep: 2,
  limitMax: 10,
  player: {
    id: 1,
    et: 100,
    lap: 1,
    lapStart: 100,
    fuel: 50,
    throttle: 0.8,
    brake: 0.2,
    clutch: 0.3,
    steering: 0.5,
    speed: 80,
    gear: 3,
    delta: -0.2,
    invalid: false,
  },
  cars: [
    {
      id: 1,
      name: "Driver",
      vehicle: "Mercedes AMG #77",
      class: "LMGT3",
      player: true,
      place: 1,
      laps: 0,
      best: 140,
      last: 141,
      distance: 100,
      countLap: 2,
      pitStops: 0,
      estimated: 140,
      behind: 0,
      lapsBehind: 0,
    },
  ],
  ...patch,
});
test("overlay preferences persist independently and reject invalid geometry", () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "sector-overlays-"));
  const file = path.join(dir, "overlays.json");
  try {
    const initial = config.defaults();
    initial.widgets.inputs.showClutch = false;
    initial.widgets.inputs.scale = 1.25;
    assert.deepEqual(config.write(file, initial), config.read(file));
    // Existing preferences from versions with Spread remain loadable.
    const previous = structuredClone(initial);
    previous.widgets["session-summary"].columns.spread = false;
    previous.widgets["session-summary"].columns.laps = false;
    const migrated = config.write(file, previous);
    assert.equal(
      Object.hasOwn(migrated.widgets["session-summary"].columns, "spread"),
      false,
    );
    assert.equal(migrated.widgets["session-summary"].columns.laps, false);
    for (const field of ["scale", "x", "opacity"]) {
      const bad = structuredClone(initial);
      bad.widgets.inputs[field] = Infinity;
      assert.throws(() => config.validate(bad));
    }
    const bad = structuredClone(initial);
    bad.widgets.inputs.accent = "url(http://invalid)";
    assert.throws(() => config.validate(bad));
    assert.equal(config.defaults().enabled, false);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
test("overlay positions clamp to negative-origin and smaller displays", () => {
  const widget = {
    ...config.defaults().widgets.inputs,
    x: 19000,
    y: 19000,
    scale: 2,
  };
  const display = { x: -1920, y: 0, width: 600, height: 400 };
  const box = config.bounds("inputs", widget, display);
  assert.equal(box.x, -1920);
  assert.equal(box.width, 600);
  assert.ok(box.y + box.height <= 400);
});
test("inputs use measured saved-speed changes and remove clutch completely", () => {
  const state = new model.TelemetryModel();
  const sample = state.update(raw());
  const settings = { ...config.defaults().widgets.inputs, showClutch: false };
  const values = model.valuesFor("inputs", sample, settings);
  assert.equal(values.speed, 80);
  assert.equal(values.pedals.length, 2);
  assert.equal(values.clutchPts, "");
  assert.equal(values.gear, 3);
  const missing = model.valuesFor("inputs", null, settings);
  assert.equal(missing.speed, "-");
  assert.equal(missing.throttlePts, "");
});
test("lap history rejects partial laps and resets after rewind or session changes", () => {
  const state = new model.TelemetryModel();
  let a = raw();
  a.player.et = 110;
  state.update(a);
  let b = raw({ et: 240 });
  b.player = { ...b.player, et: 240, lap: 2, lapStart: 240, fuel: 48 };
  b.cars[0].last = 140;
  b.cars[0].laps = 1;
  let next = state.update(b);
  assert.equal(next.laps.length, 1);
  assert.equal(next.laps[0].clean, false);
  assert.equal(next.laps[0].fuel, null);
  let c = structuredClone(b);
  c.player = { ...c.player, et: 380, lap: 3, lapStart: 380, fuel: 46 };
  c.et = 380;
  c.cars[0].laps = 2;
  next = state.update(c);
  assert.equal(next.laps[0].clean, true);
  assert.equal(next.laps[0].fuel, 2);
  next = state.update(b);
  assert.equal(next.laps.length, 0);
  next = state.update({ ...raw(), track: "Monza" });
  assert.equal(next.laps.length, 0);
  state.update(null);
  assert.equal(state.history.length, 0);
});
test("weather never invents forecast or humidity and relative marks estimated gaps", () => {
  const settings = config.defaults().widgets.weather;
  const frame = new model.TelemetryModel().update(raw());
  const weather = model.valuesFor("weather", frame, settings);
  assert.equal(weather.humidity, "-");
  assert.equal(weather.slots[1].rain, "-");
  frame.cars.push({
    ...frame.cars[0],
    id: 2,
    player: false,
    place: 2,
    distance: 90,
  });
  const relative = model.valuesFor("relative", frame, settings);
  assert.match(relative.rows.find((r) => r.id === 2).gapTxt, /^~/);
  const absent = model.valuesFor("leaderboard", null, settings);
  assert.deepEqual(absent.classes, []);
});

import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import telemetry from "../main/overlay-telemetry.cjs";
test("telemetry expires a frozen SDK frame and stops cooperatively", async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "sector-feed-"));
  fs.mkdirSync(path.join(dir, "bin"));
  fs.writeFileSync(path.join(dir, "bin/overlay-reader.exe"), "fixture");
  let now = 1000;
  const child = new EventEmitter();
  child.stdin = new PassThrough();
  child.stdout = new PassThrough();
  const feed = new telemetry.OverlayTelemetry(dir, {
    now: () => now,
    discover: async () => [{ pid: 123 }],
    spawnChild: () => child,
  });
  try {
    feed.stopped = false;
    await feed.tick();
    child.stdout.write(JSON.stringify(raw()) + "\n");
    assert.equal(feed.frame.player.speed, 80);
    now += 1600;
    child.stdout.write(JSON.stringify(raw()) + "\n");
    await feed.tick();
    assert.equal(feed.frame, null);
    const next = raw();
    next.et++;
    next.player.et++;
    child.stdout.write(JSON.stringify(next) + "\n");
    assert.equal(feed.frame.player.speed, 80);
    feed.stop();
    assert.equal(child.stdin.writableEnded, true);
    assert.equal(feed.frame, null);
  } finally {
    feed.stop();
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
test("quit during telemetry discovery never spawns a reader", async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "sector-discovery-"));
  fs.mkdirSync(path.join(dir, "bin"));
  fs.writeFileSync(path.join(dir, "bin/overlay-reader.exe"), "fixture");
  let resolve,
    spawns = 0;
  const feed = new telemetry.OverlayTelemetry(dir, {
    discover: () =>
      new Promise((r) => {
        resolve = r;
      }),
    spawnChild: () => {
      spawns++;
    },
  });
  try {
    feed.stopped = false;
    const tick = feed.tick();
    feed.stop();
    resolve([{ pid: 1 }]);
    await tick;
    assert.equal(spawns, 0);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

test("lap timing waits for delayed scoring and excludes the zero-numbered pit lap", () => {
  const state = new model.TelemetryModel();
  const start = raw();
  start.player.lap = 0;
  start.cars[0].pit = true;
  state.update(start);
  const boundary = raw();
  boundary.player = {
    ...boundary.player,
    lap: 1,
    lapStart: 240,
    et: 240,
    fuel: 48,
  };
  boundary.et = 240;
  let frame = state.update(boundary);
  assert.equal(frame.laps.length, 0);
  const scoring = structuredClone(boundary);
  scoring.cars[0].laps = 1;
  scoring.cars[0].last = 140;
  frame = state.update(scoring);
  assert.equal(frame.laps.length, 1);
  assert.equal(frame.laps[0].time, 140);
  assert.equal(frame.laps[0].clean, false);
  assert.equal(frame.laps[0].tag, "PIT");
});

test("hidden columns persist and shrink the native window, absent classes stay absent", () => {
  const value = config.defaults();
  value.widgets.leaderboard.columns = { number: false };
  value.widgets.leaderboard.showLogos = false;
  const saved = config.validate(value);
  const bounds = config.bounds("leaderboard", saved.widgets.leaderboard, {
    x: 0,
    y: 0,
    width: 1920,
    height: 1080,
  });
  assert.equal(bounds.width, 520 - 42 - 36);
  const live = model.valuesFor("leaderboard", raw(), saved.widgets.leaderboard);
  assert.deepEqual(
    live.classes.map((c) => c.badge),
    ["GT3"],
  );
  assert.equal(live.classes[0].rows[0].name, "Driver");
  const legacy = config.defaults();
  delete legacy.widgets.leaderboard.columns;
  assert.equal(
    config.validate(legacy).widgets.leaderboard.columns.driver,
    true,
  );
});
