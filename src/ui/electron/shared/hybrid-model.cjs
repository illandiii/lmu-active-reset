const finite = Number.isFinite;
const available = (h) => [1, 2, 3].includes(h?.state);
const battery = (h) =>
  available(h) && finite(h.battery) && h.battery >= 0 && h.battery <= 1
    ? h.battery
    : null;
const mapKnown = (h) =>
  Number.isInteger(h?.map) &&
  Number.isInteger(h?.mapMax) &&
  h.mapMax > 0 &&
  h.map >= 0 &&
  h.map <= h.mapMax &&
  h.mapMax <= 255;

class HybridHistory {
  constructor() {
    this.start = null;
    this.previous = null;
    this.laps = [];
  }
  update(raw) {
    const p = raw.player,
      h = p?.hybrid,
      charge = battery(h);
    const car = raw.cars?.find((c) => c.id === p?.id);
    if (!p || !available(h) || charge === null || !raw.driving) {
      this.start = null;
      this.previous = p?.lap ?? null;
      return this.laps;
    }
    if (this.previous !== null && p.lap !== this.previous) {
      if (
        this.start &&
        p.lap === this.start.lap + 1 &&
        this.start.clean &&
        car?.countLap === 2 &&
        h.map === this.start.map
      ) {
        this.laps.unshift({
          map: h.map,
          delta: (charge - this.start.charge) * 100,
          seconds: p.et - this.start.time,
        });
        this.laps = this.laps.slice(0, 40);
      }
      this.start = null;
    }
    if (!this.start && p.et - p.lapStart >= 0 && p.et - p.lapStart < 0.5)
      this.start = {
        lap: p.lap,
        time: p.et,
        charge,
        map: h.map,
        clean: mapKnown(h),
      };
    if (
      this.start &&
      (p.invalid || car?.pit || this.start.map !== h.map || !mapKnown(h))
    )
      this.start.clean = false;
    this.previous = p.lap;
    return this.laps;
  }
}

function hybridValues(data, settings) {
  const h = data?.connected ? data.player?.hybrid : null;
  const supported = available(h),
    charge = battery(h);
  const current = supported && mapKnown(h) ? h.map : null;
  // Mechanical power in kW from the SDK motor shaft torque and speed.
  const power =
    supported && finite(h.torque) && finite(h.rpm)
      ? Math.abs((h.torque * h.rpm * 2 * Math.PI) / 60000)
      : null;
  const mode = !supported
    ? "N/A"
    : h.state === 2
      ? "DEPLOY"
      : h.state === 3
        ? "REGEN"
        : "IDLE";
  const kw = h?.state === 1 ? 0 : power;
  const records = data?.connected ? data.hybridLaps || [] : [];
  const stats = new Map();
  for (const lap of records) {
    if (
      !Number.isInteger(lap.map) ||
      !finite(lap.delta) ||
      !finite(lap.seconds) ||
      lap.seconds <= 0
    )
      continue;
    const samples = stats.get(lap.map) || [];
    if (samples.length < 5) samples.push(lap);
    stats.set(lap.map, samples);
  }
  const mean = (samples, key) =>
    samples.reduce((sum, s) => sum + s[key], 0) / samples.length;
  const own = stats.get(current) || [];
  const trend = own.length ? mean(own, "delta") : null;
  let suggested = null;
  // Recommend only maps actually observed over complete clean laps at similar pace.
  // The SDK map is an index; never invent LMH/LMDh power steps or per-step gains.
  if (current !== null && charge !== null && own.length >= 2) {
    const pace = mean(own, "seconds");
    const candidates = [...stats].filter(
      ([map, laps]) =>
        map >= 0 &&
        map <= h.mapMax &&
        laps.length >= 2 &&
        Math.abs(mean(laps, "seconds") / pace - 1) <= 0.05,
    );
    if (candidates.length >= 2) {
      candidates.sort(
        (a, b) =>
          Math.abs(
            charge * 100 +
              mean(a[1], "delta") * settings.targetLaps -
              settings.targetSoc,
          ) -
          Math.abs(
            charge * 100 +
              mean(b[1], "delta") * settings.targetLaps -
              settings.targetSoc,
          ),
      );
      suggested = candidates[0][0];
    }
  }
  // Large SDK map ranges use a compact visual scale without inventing valid steps.
  const stepCount = current === null ? 0 : Math.min(16, h.mapMax + 1);
  const marker = (value) =>
    value === null ? -1 : Math.round((value / h.mapMax) * (stepCount - 1));
  return {
    state: mode,
    stateBg:
      mode === "DEPLOY"
        ? "#4C8DFF"
        : mode === "REGEN"
          ? "#3DDC84"
          : "rgba(255,255,255,0.12)",
    stateFg: mode === "DEPLOY" || mode === "REGEN" ? "#0D1220" : "#C2C9D0",
    kw: kw === null ? "-" : Math.round(kw),
    deploy:
      h?.state === 2 && kw !== null ? Math.min(1, kw / settings.powerScale) : 0,
    regen:
      h?.state === 3 && kw !== null ? Math.min(1, kw / settings.powerScale) : 0,
    socPct: charge === null ? "-" : Math.round(charge * 100),
    socW: charge === null ? 0 : 12 * charge,
    trend:
      trend === null
        ? "-"
        : `${trend >= 0 ? "+" : "-"}${Math.abs(trend).toFixed(1)}%`,
    trendFg: trend < 0 ? "#FF8A80" : "#7EE2A8",
    current: current ?? "-",
    suggested: suggested === null ? "-" : `~${suggested}`,
    targetSoc: settings.targetSoc,
    steps:
      current === null
        ? []
        : Array.from({ length: stepCount }, (_, i) => ({
            bg: i === marker(suggested) ? settings.accent : "rgba(0,0,0,0.5)",
            border:
              i === marker(current)
                ? "1px solid #F2F4F6"
                : "1px solid rgba(255,255,255,0.15)",
          })),
  };
}
module.exports = { HybridHistory, hybridValues };
