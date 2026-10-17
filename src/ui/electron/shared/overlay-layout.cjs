const definitions = require("./overlay-definitions.json");
const columns = require("./overlay-columns.json");
const visible = (settings, key) =>
  settings.columns?.[key] !== false && (key !== "logo" || settings.showLogos);
function dimensions(id, settings, values) {
  const def = definitions[id];
  const removed = (columns[id] || [])
    .filter(([key]) => !visible(settings, key))
    .reduce((sum, [, , width]) => sum + width, 0);
  let height = def.height;
  let width = Math.max(160, def.width - removed);
  if (id === "tyres") {
    const count = 4;
    const cols = Math.min(2, Math.max(1, count));
    width =
      24 + cols * (164 - (visible(settings, "heat") ? 0 : 48)) + (cols - 1) * 8;
    const tileHeight =
      104 -
      (visible(settings, "wear") ? 0 : 34) -
      (visible(settings, "temperature") || visible(settings, "age") ? 0 : 14);
    const rows = Math.ceil(count / 2);
    height = 46 + rows * tileHeight + Math.max(0, rows - 1) * 8;
  }
  if (id === "hybrid") {
    height =
      114 -
      (visible(settings, "bar") ? 0 : 20) -
      (visible(settings, "recommendation") ? 0 : 36);
  }
  if (id === "stint-planner") {
    const rows = ["energy", "fuel"].filter((key) =>
      visible(settings, key),
    ).length;
    height = 176 - (2 - rows) * 23;
    if (
      !["energyRate", "target", "fuelRate"].some((key) =>
        visible(settings, key),
      )
    )
      height -= 45;
  }
  if (id === "leaderboard" && values?.classes)
    height =
      68 +
      values.classes.reduce(
        (sum, c) =>
          sum + 40 + c.rows.reduce((h, r) => h + (r.sep ? 10 : 26), 0),
        0,
      );
  return { ...def, width: Math.max(160, width), height };
}
module.exports = { columns, visible, dimensions };
