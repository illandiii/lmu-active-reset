import test from "node:test";
import assert from "node:assert/strict";
import config from "../shared/overlay-config.cjs";
import model from "../shared/overlay-model.cjs";
import hybrid from "../shared/hybrid-model.cjs";
import platform from "../shared/hybrid-platform.cjs";
const settings = config.defaults().widgets;
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
    model: "Porsche 963",
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
    { map: 40, platform: "LMDh", delta: -1.4, seconds: 120 },
    { map: 40, platform: "LMDh", delta: -1.4, seconds: 120 },
    { map: 30, platform: "LMDh", delta: 1, seconds: 121 },
    { map: 30, platform: "LMDh", delta: 1, seconds: 121 },
  ];
  v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.trend, "-1.4%");
  assert.equal(v.current, 40);
  assert.equal(v.platform, "LMDh");
  assert.equal(v.suggested, "~30");
  raw.player.hybrid.map = 40;
  raw.player.hybrid.mapMax = 50;
  v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.current, 40);
  assert.equal(v.suggested, "~30");
  assert.equal("steps" in v, false);
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
  assert.equal(v.current, "-");
});
test("hybrid refuses unlearned or incomparable maps and invalid battery readings", () => {
  const raw = hybridFrame();
  raw.hybridLaps = [
    { map: 40, platform: "LMDh", delta: -1, seconds: 120 },
    { map: 40, platform: "LMDh", delta: -1, seconds: 120 },
    { map: 30, platform: "LMDh", delta: 1, seconds: 150 },
    { map: 30, platform: "LMDh", delta: 1, seconds: 150 },
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
  assert.equal(state.laps[0].map, 40);
  assert.equal(state.laps[0].platform, "LMDh");
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
  raw.player.model = "BMW M Hybrid V8";
  assert.equal(state.update(raw).length, 0);
  const combined = new model.TelemetryModel();
  combined.hybrid = state;
  combined.reset();
  assert.equal(combined.hybrid.laps.length, 0);
});

test("deployment scales follow the actual car and accept both SDK encodings", () => {
  for (const [models, name, max, step] of [
    [
      [
        "Porsche 963",
        "Cadillac V-Series.R",
        "BMW M Hybrid V8",
        "Alpine A424",
        "Lamborghini SC63",
      ],
      "LMDh",
      50,
      10,
    ],
    [
      [
        "Ferrari 499P",
        "Toyota GR010",
        "Peugeot 9X8",
        "Isotta Fraschini Tipo 6 LMH",
      ],
      "LMH",
      200,
      20,
    ],
  ]) {
    for (const model of models) {
      const profile = platform.hybridPlatform({ player: { model } });
      assert.equal(profile.name, name);
      for (let i = 0; i <= max / step; i++) {
        assert.equal(
          platform.deployMap({ map: i, mapMax: max / step }, profile),
          i * step,
        );
        assert.equal(
          platform.deployMap({ map: i * step, mapMax: max }, profile),
          i * step,
        );
      }
      for (const map of [-1, 7, max + step, null])
        assert.equal(platform.deployMap({ map, mapMax: max }, profile), null);
      assert.equal(platform.deployMap({ map: 1, mapMax: 99 }, profile), null);
    }
  }
  assert.equal(
    platform.hybridPlatform({ player: { model: "Ferrari 296 GT3" } }),
    null,
  );
  assert.equal(
    platform.hybridPlatform({
      player: { id: 1 },
      cars: [{ id: 2, vehicle: "Porsche 963" }],
    }),
    null,
  );
  assert.equal(
    platform.hybridPlatform({
      player: { id: 1 },
      cars: [{ id: 1, vehicle: "Porsche 963 #6" }],
    }).name,
    "LMDh",
  );
});

test("LMH recommendations stay on 20-unit steps and reject other platform history", () => {
  const raw = hybridFrame();
  raw.player.model = "Ferrari 499P";
  raw.player.hybrid.map = 8;
  raw.player.hybrid.mapMax = 10;
  raw.hybridLaps = [160, 160, 140, 140].map((map) => ({
    map,
    platform: "LMH",
    delta: map === 160 ? -1.4 : 1,
    seconds: 120,
  }));
  const v = hybrid.hybridValues(raw, settings.hybrid);
  assert.equal(v.platform, "LMH");
  assert.equal(v.current, 160);
  assert.equal(v.suggested, "~140");
  raw.hybridLaps.forEach((lap) => {
    lap.platform = "LMDh";
  });
  assert.equal(hybrid.hybridValues(raw, settings.hybrid).suggested, "-");
  raw.player.model = "Unknown";
  assert.equal(hybrid.hybridValues(raw, settings.hybrid).current, "-");
});
