import test from "node:test";
import assert from "node:assert/strict";
import config from "../shared/overlay-config.cjs";
import model from "../shared/overlay-model.cjs";
import wheels from "../shared/wheel-model.cjs";
import stint from "../shared/stint-model.cjs";
import telemetry from "../main/overlay-telemetry.cjs";
const settings = config.defaults().widgets;
const wheel = (patch = {}) => ({
  attached: true,
  flat: false,
  load: 3000,
  groundSpeed: 30,
  rotation: 100,
  radius: 0.31,
  deflection: 0.01,
  surface: 0,
  wear: 0.8,
  temperatures: [98, 93, 88],
  compoundIndex: 0,
  ...patch,
});
const frame = () => ({
  connected: true,
  driving: true,
  track: "Spa",
  session: 1,
  player: {
    id: 1,
    et: 100,
    lapStart: 100,
    lap: 1,
    brake: 0.84,
    speed: 108,
    frontCompound: "Soft",
    rearCompound: "Medium",
    fuel: 38.6,
    energy: 0.412,
    wheels: [wheel({ rotation: 45 }), wheel(), wheel(), wheel()],
  },
  cars: [{ id: 1, laps: 48, distance: 100, countLap: 2 }],
  laps: [{ clean: true, energy: 3.6, fuel: 2.42 }],
});

test("old overlay layouts gain disabled widgets without losing existing preferences", () => {
  const old = config.defaults();
  old.widgets.inputs.x = 123;
  for (const id of ["brake-lock", "tyres", "stint-planner"])
    delete old.widgets[id];
  const migrated = config.validate(old);
  assert.equal(migrated.widgets.inputs.x, 123);
  assert.equal(Object.keys(migrated.widgets).length, 11);
  assert.equal(migrated.widgets.tyres.enabled, false);
  assert.equal(migrated.widgets["brake-lock"].slipThreshold, 20);
  const bad = structuredClone(migrated);
  bad.widgets.tyres.hotTemp = 70;
  assert.throws(() => config.validate(bad), /Cold temperature/);
  bad.widgets.tyres.hotTemp = 105;
  bad.widgets["stint-planner"].consumptionLaps = -1;
  assert.throws(() => config.validate(bad), /consumptionLaps/);
});

test("lock detection requires braking and loaded wheels, respects individual ground speed", () => {
  const raw = frame();
  let values = wheels.brakeValues(raw, settings["brake-lock"]);
  assert.deepEqual(
    values.tyres.map((w) => w.locked),
    [true, false, false, false],
  );
  for (const patch of [
    { load: 0 },
    { groundSpeed: 1 },
    { radius: 0 },
    { attached: false },
    { surface: 3 },
    { rotation: null },
  ]) {
    raw.player.wheels[0] = wheel({ rotation: 45, ...patch });
    assert.equal(
      wheels.brakeValues(raw, settings["brake-lock"]).tyres[0].locked,
      false,
    );
  }
  raw.player.wheels[0] = wheel({ rotation: 60, groundSpeed: 18 });
  assert.equal(
    wheels.brakeValues(raw, settings["brake-lock"]).tyres[0].locked,
    false,
  );
  raw.player.wheels[0] = wheel({ rotation: 45 });
  raw.player.brake = 0;
  assert.equal(
    wheels.brakeValues(raw, settings["brake-lock"]).tyres[0].locked,
    false,
  );
  raw.player.brake = 0.84;
  assert.equal(
    wheels.brakeValues(raw, { ...settings["brake-lock"], slipThreshold: 80 })
      .tyres[0].locked,
    false,
  );
});

test("lock events and observed tyre age reset after rewind and tyre replacement", () => {
  const state = new model.TelemetryModel();
  const raw = frame();
  let data = state.update(structuredClone(raw));
  assert.equal(data.brakeEvents.length, 1);
  assert.deepEqual(data.tyreLaps, [null, null, null, null]);
  assert.equal(
    model.valuesFor("brake-lock", data, settings["brake-lock"]).threshold,
    "~84",
  );
  raw.player.et++;
  raw.player.wheels[0].wear = 1;
  data = state.update(structuredClone(raw));
  assert.equal(data.tyreLaps[0], 0);
  raw.player.lap++;
  raw.player.et++;
  data = state.update(structuredClone(raw));
  assert.equal(data.tyreLaps[0], 1);
  raw.player.et = 1;
  raw.player.brake = 0;
  data = state.update(structuredClone(raw));
  assert.equal(data.brakeEvents.length, 0);
  assert.deepEqual(data.tyreLaps, [null, null, null, null]);
  const reader = new telemetry.OverlayTelemetry("unused");
  reader.model = state;
  reader.publish(data, "Live");
  reader.onResetStatus({ restore_count: 1 });
  assert.equal(reader.frame, null);
  assert.equal(reader.model.previous, null);
});

test("tyres preserve side temperatures, unknown age and real puncture state", () => {
  const raw = frame();
  raw.player.wheels[0].flat = true;
  const values = wheels.tyreValues(raw, settings.tyres);
  assert.equal(values.tyres[0].avg, 93);
  assert.equal(values.tyres[0].lifePct, 80);
  assert.equal(values.tyres[0].compound, "S");
  assert.equal(values.tyres[2].compound, "M");
  assert.equal(values.tyres[0].flat, true);
  assert.equal(values.tyres[0].laps, "-");
  assert.equal(wheels.tyreValues(null, settings.tyres).tyres[0].lifePct, "-");
  raw.player.wheels[0].temperatures[1] = null;
  assert.equal(wheels.tyreValues(raw, settings.tyres).tyres[0].avg, "-");
});

test("stint estimates use clean measured consumption and both resources with reserve", () => {
  const raw = frame();
  raw.laps.unshift({ clean: false, energy: 90, fuel: 90 });
  let values = stint.stintValues(raw, settings["stint-planner"]);
  assert.equal(values.boxLaps, 11);
  assert.equal(values.boxLap, "L59");
  assert.deepEqual(
    values.res.map((r) => r.laps),
    [11, 15],
  );
  assert.equal(values.energyPer, "3.6%");
  assert.equal(values.target, "3.4%");
  values = stint.stintValues(raw, {
    ...settings["stint-planner"],
    reserveLaps: 2,
  });
  assert.equal(values.boxLaps, 9);
  raw.player.fuel = 10;
  assert.equal(stint.stintValues(raw, settings["stint-planner"]).boxLaps, 4);
  raw.player.fuel = 0;
  assert.equal(stint.stintValues(raw, settings["stint-planner"]).boxLap, "Now");
  raw.laps = [];
  assert.equal(stint.stintValues(raw, settings["stint-planner"]).boxLaps, "-");
  assert.equal(stint.stintValues(null, settings["stint-planner"]).target, "-");
});

test("hidden wheel and thermal sections shrink the widget and bound native windows", () => {
  const w = {
    ...settings.tyres,
    columns: {
      frontRight: false,
      rearRight: false,
      rearLeft: false,
      heat: false,
    },
  };
  const b = config.bounds("tyres", w, {
    x: 0,
    y: 0,
    width: 1920,
    height: 1080,
  });
  assert.equal(b.width, 264);
  assert.equal(b.height, 262);
  const slim = config.bounds(
    "brake-lock",
    { ...settings["brake-lock"], columns: { wheels: false } },
    { x: 0, y: 0, width: 1920, height: 1080 },
  );
  assert.equal(slim.width, 338);
});

function lockSequence({
  count = 60,
  step = 0.05,
  wheelPatch = {},
  playerPatch = {},
  driving = true,
} = {}) {
  const history = new wheels.WheelHistory();
  let data;
  for (let i = 0; i <= count; i++) {
    const raw = frame();
    raw.driving = driving;
    Object.assign(raw.player, { et: 100 + i * step, ...playerPatch });
    raw.player.wheels[0] = wheel({ rotation: 0, ...wheelPatch });
    data = { ...raw, ...history.update(raw) };
  }
  return { data, history };
}

test("flat spot estimates retain a sustained localized lock and respect sensitivity", () => {
  const { data, history } = lockSequence({ count: 40 });
  assert.ok(Math.abs(data.flatSpots[0].exposure - 60) < 0.001);
  assert.deepEqual(
    wheels.tyreValues(data, settings.tyres).tyres.map((w) => w.estimatedFlat),
    [true, false, false, false],
  );
  assert.equal(
    wheels.tyreValues(data, settings.tyres).tyres[0].conditionText,
    "~flat spot",
  );
  assert.equal(
    wheels.tyreValues(data, { ...settings.tyres, flatSpotSensitivity: 10 })
      .tyres[0].marked,
    false,
  );
  const rolling = frame();
  rolling.player.et = 102.05;
  Object.assign(rolling, history.update(rolling));
  assert.equal(
    wheels.tyreValues(rolling, settings.tyres).tyres[0].estimatedFlat,
    true,
  );
  rolling.player.wheels[0].flat = true;
  assert.equal(
    wheels.tyreValues(rolling, settings.tyres).tyres[0].conditionText,
    "puncture",
  );
  assert.equal(
    wheels.tyreValues(rolling, settings.tyres).tyres[0].estimatedFlat,
    false,
  );
});

test("flat spot estimates reject short, rotating, unloaded and interrupted samples", () => {
  for (const options of [
    { count: 2, wheelPatch: { groundSpeed: 100, load: 9000 } },
    { wheelPatch: { rotation: 5 } },
    { wheelPatch: { load: 0 } },
    { wheelPatch: { groundSpeed: 1 } },
    { wheelPatch: { attached: false } },
    { wheelPatch: { surface: 4 } },
    { wheelPatch: { rotation: null } },
    { wheelPatch: { flat: true } },
    { playerPatch: { brake: 0 } },
    { driving: false },
    { step: 0 },
    { step: 1 },
  ]) {
    const { data } = lockSequence(options);
    assert.equal(
      wheels.tyreValues(data, settings.tyres).tyres[0].estimatedFlat,
      false,
      JSON.stringify(options),
    );
  }
  const history = new wheels.WheelHistory();
  let data;
  for (let i = 0; i < 100; i++) {
    const raw = frame();
    raw.player.et += i * 0.05;
    raw.player.wheels[0].rotation = i % 3 ? 0 : 100;
    data = { ...raw, ...history.update(raw) };
  }
  assert.equal(wheels.tyreValues(data, settings.tyres).tyres[0].marked, false);
});

test("flat spot integration uses game time, wheel load and clears on tyre change or rewind", () => {
  const slow = lockSequence({ count: 20, step: 0.1 }).data;
  const fast = lockSequence({ count: 100, step: 0.02 }).data;
  assert.ok(
    Math.abs(slow.flatSpots[0].exposure - fast.flatSpots[0].exposure) < 0.001,
  );
  const light = lockSequence({ count: 40, wheelPatch: { load: 1000 } }).data;
  assert.equal(wheels.tyreValues(light, settings.tyres).tyres[0].marked, false);
  for (const patch of [{ wear: 1 }, { compoundIndex: 1 }]) {
    const { data, history } = lockSequence();
    const raw = structuredClone(data);
    raw.player.et += 0.05;
    Object.assign(raw.player.wheels[0], patch);
    assert.equal(history.update(raw).flatSpots[0].exposure, 0);
  }
  const { data, history } = lockSequence();
  const dose = data.flatSpots[0].exposure;
  for (let i = 0; i < 10; i++)
    assert.equal(history.update(data).flatSpots[0].exposure, dose);
  data.player.et = 1;
  assert.equal(history.update(data).flatSpots[0].exposure, 0);
  // Same-compound replacement can restore only a small amount of tyre life.
  const fresh = lockSequence({ wheelPatch: { wear: 0.99 } });
  const changed = structuredClone(fresh.data);
  changed.cars[0].pit = true;
  changed.player.speed = 0;
  changed.player.et += 0.05;
  changed.player.wheels[0] = wheel({ wear: 1 });
  assert.equal(fresh.history.update(changed).flatSpots[0].exposure, 0);
  const invalid = config.defaults();
  invalid.widgets.tyres.flatSpotSensitivity = 0;
  assert.throws(() => config.validate(invalid), /flatSpotSensitivity/);
});
