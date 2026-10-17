const fs = require("node:fs");
const path = require("node:path");
const { randomUUID } = require("node:crypto");
const layout = require("./overlay-layout.cjs");
const definitions = require("./overlay-definitions.json");
function defaults() {
  return {
    enabled: false,
    displayId: null,
    widgets: Object.fromEntries(
      Object.keys(definitions).map((id, i) => [
        id,
        {
          enabled: ["inputs", "delta"].includes(id),
          columns: {},
          x: 30 + (i % 3) * 460,
          y: 30 + Math.floor(i / 3) * 330,
          scale: 1,
          opacity: id === "leaderboard" ? 0.1 : 0.35,
          accent: "#FFD23F",
          rows: id === "laps" ? 8 : 7,
          showClutch: true,
          showTrace: true,
          showWheel: true,
          showGear: true,
          showLogos: true,
          lockDeg: 270,
          ...(id === "brake-lock"
            ? { slipThreshold: 20, minimumSpeed: 20, predictionMs: 500 }
            : {}),
          ...(id === "tyres"
            ? { coldTemp: 80, hotTemp: 105, flatSpotSensitivity: 50 }
            : {}),
          ...(id === "stint-planner"
            ? { consumptionLaps: 5, reserveLaps: 0 }
            : {}),
          ...(id === "hybrid"
            ? { targetSoc: 50, targetLaps: 5, powerScale: 220 }
            : {}),
        },
      ]),
    ),
  };
}
function validate(value) {
  if (!value || typeof value.enabled !== "boolean" || !value.widgets)
    throw new Error("Invalid overlay settings.");
  if (value.displayId !== null && !Number.isInteger(value.displayId))
    throw new Error("Invalid display.");
  const result = defaults();
  result.enabled = value.enabled;
  result.displayId = value.displayId;
  for (const id of Object.keys(definitions)) {
    const w = value.widgets[id];
    if (!w) {
      // Older installations do not have entries for newly added widgets.
      if (["brake-lock", "tyres", "stint-planner", "hybrid"].includes(id))
        continue;
      throw new Error("Missing widget settings.");
    }
    for (const key of [
      "enabled",
      "showClutch",
      "showTrace",
      "showWheel",
      "showGear",
      "showLogos",
    ]) {
      if (typeof w[key] !== "boolean")
        throw new Error("Invalid widget visibility.");
      result.widgets[id][key] = w[key];
    }
    for (const [key, min, max] of [
      ["x", 0, 20000],
      ["y", 0, 20000],
      ["scale", 0.5, 2],
      ["opacity", 0, 1],
      ["rows", 3, 12],
      ["lockDeg", 90, 540],
    ]) {
      if (!Number.isFinite(w[key]) || w[key] < min || w[key] > max)
        throw new Error(`Invalid ${key}.`);
      result.widgets[id][key] = ["x", "y", "rows", "lockDeg"].includes(key)
        ? Math.round(w[key])
        : w[key];
    }
    if ((id === "relative" && w.rows > 7) || (id === "laps" && w.rows > 8))
      throw new Error("Too many rows for this widget.");
    if (!/^#[0-9a-f]{6}$/i.test(w.accent))
      throw new Error("Invalid accent color.");
    result.widgets[id].accent = w.accent;
    const ranges =
      id === "brake-lock"
        ? [
            ["slipThreshold", 5, 90],
            ["minimumSpeed", 5, 150],
            ["predictionMs", 100, 1000],
          ]
        : id === "tyres"
          ? [
              ["coldTemp", 0, 150],
              ["hotTemp", 20, 200],
              ["flatSpotSensitivity", 10, 100],
            ]
          : id === "stint-planner"
            ? [
                ["consumptionLaps", 1, 20],
                ["reserveLaps", 0, 5],
              ]
            : id === "hybrid"
              ? [
                  ["targetSoc", 10, 90],
                  ["targetLaps", 1, 10],
                  ["powerScale", 50, 500],
                ]
              : [];
    for (const [key, min, max] of ranges) {
      const candidate = w[key] ?? result.widgets[id][key];
      if (!Number.isInteger(candidate) || candidate < min || candidate > max)
        throw new Error(`Invalid ${key}.`);
      result.widgets[id][key] = candidate;
    }
    if (
      id === "tyres" &&
      result.widgets[id].coldTemp >= result.widgets[id].hotTemp
    )
      throw new Error("Cold temperature must be lower than hot temperature.");
    for (const [key] of layout.columns[id]) {
      const value = w.columns?.[key];
      if (value !== undefined && typeof value !== "boolean")
        throw new Error("Invalid column visibility.");
      result.widgets[id].columns[key] = value !== false;
    }
  }
  return result;
}
function read(file) {
  try {
    return validate(JSON.parse(fs.readFileSync(file, "utf8")));
  } catch (error) {
    if (error.code === "ENOENT") return defaults();
    throw new Error("Overlay settings could not be read.");
  }
}
function write(file, value) {
  const config = validate(value);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.${randomUUID()}.tmp`;
  try {
    fs.writeFileSync(tmp, JSON.stringify(config), "utf8");
    fs.renameSync(tmp, file);
  } finally {
    fs.rmSync(tmp, { force: true });
  }
  return config;
}
function bounds(id, widget, display, values) {
  const def = layout.dimensions(id, widget, values);
  // Limit scale to the available display; never strand a widget off-screen.
  const scale = Math.min(
    widget.scale,
    display.width / def.width,
    display.height / def.height,
  );
  const width = Math.round(def.width * scale),
    height = Math.round(def.height * scale);
  return {
    x:
      display.x +
      Math.max(0, Math.min(Math.round(widget.x), display.width - width)),
    y:
      display.y +
      Math.max(0, Math.min(Math.round(widget.y), display.height - height)),
    width,
    height,
    scale,
  };
}
module.exports = { definitions, defaults, validate, read, write, bounds };
