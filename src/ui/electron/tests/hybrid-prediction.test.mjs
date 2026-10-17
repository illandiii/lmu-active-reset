import test from "node:test";
import assert from "node:assert/strict";
import config from "../shared/overlay-config.cjs";
import model from "../shared/overlay-model.cjs";
import wheelModel from "../shared/wheel-model.cjs";
import hybrid from "../shared/hybrid-model.cjs";
const settings = config.defaults().widgets;
function brakeFrame(t) {
  return {
    connected: true,
    driving: true,
    cars: [],
    player: {
      id: 1,
      et: 100 + t,
      lap: 1,
      brake: 0.4 + t * 0.8,
      wheels: Array.from({ length: 4 }, (_, i) => ({
        attached: true,
        load: 3000,
        groundSpeed: 30,
        lateralSpeed: 2,
        radius: 0.31,
        deflection: 0.01,
        rotation: i === 0 ? 100 * (1 - (0.05 + t * 0.3)) : 100,
        surface: 0,
      })),
    },
  };
}
function braking() {
  const state = new wheelModel.WheelHistory();
  let raw;
  for (let i = 0; i <= 6; i++) {
    raw = brakeFrame(i * 0.05);
    Object.assign(raw, state.update(raw));
  }
  return { raw, state };
}
test("lock forecast identifies a wheel, time and pedal threshold before locking", () => {
  const { raw } = braking();
  const values = wheelModel.brakeValues(raw, settings["brake-lock"]);
  assert.equal(values.threshold, "~80");
  assert.equal(values.forecast, "FL ~200 ms");
  assert.deepEqual(
    values.tyres.map((w) => w.predicted),
    [true, false, false, false],
  );
  assert.equal(
    values.tyres.some((w) => w.locked),
    false,
  );
  raw.player.wheels[0].rotation = 50;
  assert.equal(
    wheelModel.brakeValues(raw, settings["brake-lock"]).forecast,
    "FL LOCK",
  );
});
test("lock forecast rejects changing corner conditions, missing telemetry and released brakes", () => {
  for (const alter of [
    (raw) => {
      raw.player.wheels[0].lateralSpeed = 15;
    },
    (raw) => {
      raw.player.wheels[0].lateralSpeed = null;
    },
    (raw) => {
      raw.player.wheels[0].groundSpeed = 10;
      raw.player.wheels[0].rotation = 28.667;
    },
    (raw) => {
      raw.player.wheels[0].load = 600;
    },
    (raw) => {
      raw.player.brake = 0;
    },
    (raw) => {
      raw.player.et += 1;
    },
    (raw) => {
      raw.driving = false;
    },
  ]) {
    const { raw } = braking();
    alter(raw);
    assert.equal(
      wheelModel.brakeValues(raw, settings["brake-lock"]).tyres[0].predicted,
      false,
    );
  }
  const { raw, state } = braking();
  assert.equal(
    wheelModel.brakeValues(raw, {
      ...settings["brake-lock"],
      predictionMs: 100,
    }).forecast,
    "-",
  );
  assert.equal(
    state.update(raw).brakeTrace[0].length,
    raw.brakeTrace[0].length,
  );
  raw.player.et += 1;
  Object.assign(raw, state.update(raw));
  assert.equal(
    wheelModel.brakeValues(raw, settings["brake-lock"]).forecast,
    "-",
  );
});
test("leaderboard keeps player neighbours under their class with matching resources", () => {
  for (const own of ["Hypercar", "LMP2", "LMP3", "LMGT3"]) {
    const cars = ["Hypercar", "LMP2", "LMP3", "LMGT3"].flatMap((cls, c) =>
      Array.from({ length: 12 }, (_, i) => ({
        id: c * 20 + i,
        class: cls,
        place: c * 12 + i + 1,
        player: cls === own && i === 9,
        name: `${cls}-${i}`,
        vehicle: "Ferrari #50",
        laps: 5,
        lapsBehind: 0,
        behind: i,
        resources:
          i === 9
            ? { energy: 0.42, compounds: ["Soft", "Soft", "Medium", "Medium"] }
            : null,
      })),
    );
    const values = model.valuesFor(
      "leaderboard",
      {
        connected: true,
        session: 10,
        player: { id: cars.find((c) => c.player).id },
        cars,
      },
      settings.leaderboard,
    );
    const group = values.classes.find((c) =>
      c.rows.some((r) => r.name === `${own}-9`),
    );
    assert.equal(
      group.badge,
      { Hypercar: "HY", LMP2: "P2", LMP3: "P3", LMGT3: "GT3" }[own],
    );
    assert.equal(
      values.classes.filter((c) => c.rows.some((r) => r.name === `${own}-9`))
        .length,
      1,
    );
    assert.ok(group.rows.some((r) => r.sep));
    const player = group.rows.find((r) => r.name === `${own}-9`);
    assert.equal(player.veTxt, "42%");
    assert.equal(player.mixed, true);
    assert.equal(group.rows[0].veTxt, "-");
    assert.equal(values.classes[0].badge, "HY");
  }
});
test("old tyre visibility is discarded and all four tyres keep their geometry", () => {
  const old = config.defaults();
  delete old.widgets.hybrid;
  old.widgets.tyres.columns = {
    frontLeft: false,
    frontRight: false,
    rearLeft: false,
    rearRight: false,
  };
  const migrated = config.validate(old);
  assert.equal(migrated.widgets.tyres.columns.frontLeft, undefined);
  assert.equal(migrated.widgets.hybrid.enabled, false);
  assert.equal(
    config.bounds("tyres", old.widgets.tyres, {
      x: 0,
      y: 0,
      width: 1920,
      height: 1080,
    }).width,
    360,
  );
});
const hybridFrame = () => ({
  connected: true,
  driving: true,
  cars: [{ id: 1, countLap: 2 }],
  player: {
    id: 1,
    et: 0,
    lapStart: 0,
    lap: 1,
    hybrid: {
      state: 2,
      battery: 0.45,
      torque: 120,
      rpm: 10000,
      map: 4,
      mapMax: 5,
    },
  },
});
test("hybrid uses actual state, shaft power, battery and learned map observations", () => {
  const raw = hybridFrame();
  let v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.state, "DEPLOY");
  assert.equal(v.kw, 126);
  assert.equal(v.socPct, 45);
  assert.equal(v.trend, "-");
  assert.equal(v.suggested, "-");
  raw.hybridLaps = [
    { map: 4, delta: -1.4, seconds: 120 },
    { map: 4, delta: -1.4, seconds: 120 },
    { map: 3, delta: 1, seconds: 121 },
    { map: 3, delta: 1, seconds: 121 },
  ];
  v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.trend, "-1.4%");
  assert.equal(v.suggested, "~3");
  raw.player.hybrid.map = 40;
  raw.player.hybrid.mapMax = 50;
  raw.hybridLaps = raw.hybridLaps.map((l) => ({ ...l, map: l.map * 10 }));
  v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.current, 40);
  assert.equal(v.suggested, "~30");
  assert.equal(v.steps.length, 16);
  raw.player.hybrid.state = 3;
  v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.state, "REGEN");
  assert.ok(v.regen > 0);
  assert.equal(v.deploy, 0);
  raw.player.hybrid.state = 0;
  v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.state, "N/A");
  assert.equal(v.socPct, "-");
  assert.equal(v.kw, "-");
  assert.equal(v.steps.length, 0);
});
test("hybrid refuses unlearned or incomparable maps and invalid battery readings", () => {
  const raw = hybridFrame();
  raw.hybridLaps = [
    { map: 4, delta: -1, seconds: 120 },
    { map: 4, delta: -1, seconds: 120 },
    { map: 3, delta: 1, seconds: 150 },
    { map: 3, delta: 1, seconds: 150 },
  ];
  assert.equal(hybrid.hybridValues(raw, settings.hybrid).suggested, "-");
  raw.player.hybrid.battery = null;
  raw.player.hybrid.torque = null;
  const v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.socPct, "-");
  assert.equal(v.kw, "-");
});
test("hybrid trend records complete clean fixed-map laps and clears with the model", () => {
  const state = new hybrid.HybridHistory();
  const raw = hybridFrame();
  state.update(structuredClone(raw));
  raw.player.et = 120;
  raw.player.lapStart = 120;
  raw.player.lap = 2;
  raw.player.hybrid.battery = 0.43;
  assert.ok(Math.abs(state.update(structuredClone(raw))[0].delta + 2) < 0.001);
  raw.player.et = 180;
  raw.player.invalid = true;
  state.update(structuredClone(raw));
  raw.player.et = 240;
  raw.player.lapStart = 240;
  raw.player.lap = 3;
  raw.player.invalid = false;
  assert.equal(state.update(structuredClone(raw)).length, 1);
  raw.player.et = 260;
  raw.player.hybrid.map = 3;
  state.update(structuredClone(raw));
  raw.player.et = 360;
  raw.player.lapStart = 360;
  raw.player.lap = 4;
  assert.equal(state.update(raw).length, 1);
  const combined = new model.TelemetryModel();
  combined.hybrid = state;
  combined.reset();
  assert.equal(combined.hybrid.laps.length, 0);
});
