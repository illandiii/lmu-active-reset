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
  if (id === "leaderboard" && values?.classes)
    height =
      68 + values.classes.reduce((sum, c) => sum + 40 + c.rows.length * 26, 0);
  return { ...def, width: Math.max(160, def.width - removed), height };
}
module.exports = { columns, visible, dimensions };
