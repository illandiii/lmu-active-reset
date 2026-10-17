function stintValues(data, settings) {
  const p = data?.connected ? data.player : null;
  const car = data?.connected ? data.cars?.find((c) => c.id === p?.id) : null;
  const laps = (data?.connected ? data.laps || [] : [])
    .filter((lap) => lap.clean)
    .slice(0, settings.consumptionLaps ?? 5);
  const average = (key) => {
    const values = laps
      .map((lap) => lap[key])
      .filter((v) => Number.isFinite(v) && v > 0);
    return values.length
      ? values.reduce((a, b) => a + b, 0) / values.length
      : null;
  };
  const energyPer = average("energy"),
    fuelPer = average("fuel");
  const energy =
    Number.isFinite(p?.energy) && p.energy >= 0 && p.energy <= 1
      ? p.energy * 100
      : null;
  const fuel = Number.isFinite(p?.fuel) && p.fuel >= 0 ? p.fuel : null;
  const energyLaps = energy !== null && energyPer ? energy / energyPer : null;
  const fuelLaps = fuel !== null && fuelPer ? fuel / fuelPer : null;
  // Both resources constrain the stint. Missing consumption must not imply unlimited range.
  const remaining =
    energyLaps !== null && fuelLaps !== null
      ? Math.min(energyLaps, fuelLaps)
      : null;
  const reserve = settings.reserveLaps ?? 0;
  const box =
    remaining === null ? null : Math.max(0, Math.floor(remaining) - reserve);
  const max = Math.max(energyLaps ?? 0, fuelLaps ?? 0, 1);
  const row = (key, label, value, color) => ({
    key,
    label,
    color,
    laps: value === null ? "-" : Math.max(0, Math.floor(value)),
    frac: value === null ? 0 : Math.min(1, value / max),
    labelFg: value !== null && value === remaining ? "#FFC062" : "#C2C9D0",
    lapsFg: value !== null && value === remaining ? "#FFC062" : "#F2F4F6",
  });
  return {
    res: [
      row("energy", "VE", energyLaps, "#8FBBFF"),
      row("fuel", "FUEL", fuelLaps, "#C2C9D0"),
    ],
    boxLaps: box ?? "-",
    boxLap:
      box !== null && Number.isFinite(car?.laps)
        ? box === 0
          ? "Now"
          : `L${car.laps + box}`
        : "-",
    energyPer: energyPer === null ? "-" : `${energyPer.toFixed(1)}%`,
    fuelPer: fuelPer === null ? "-" : `${fuelPer.toFixed(2)} L`,
    target:
      energy !== null && energyPer
        ? `${(energy / (Math.floor(energy / energyPer) + 1)).toFixed(1)}%`
        : "-",
  };
}
module.exports = { stintValues };
