import samples from "./samples.json";
const common = {
  sessionName: "RACE",
  trackCars: "Spa-Francorchamps \u00b7 56 cars",
  lap: 48,
  remaining: "2:14:37",
  elapsed: "3:45:23",
  best: "2:18.412",
  surface: "Dry track \u00b7 grip 98%",
  air: "17\u00b0",
  trackTemp: "24\u00b0",
  humidity: "68%",
  forecast: "Rain likely in ~1h 20m",
  rainLap: "\u2248 lap 82",
  stint: "Stint 3 \u00b7 5 laps",
  energy: "41.2%",
  fuel: "38.6 L",
  bestLapLabel: "L31",
  bestEnergy: "3.5%",
  bestFuel: "2.40 L",
  bestGap: "+0.793",
  average: "2:18.864",
  averageEnergy: "3.6%",
  averageFuel: "2.42 L",
  averageGap: "+0.452",
  sessionTrack: "6 Hours Race \u00b7 Spa-Francorchamps",
  classBadge: "GT3",
  classColor: "#1FA34A",
  progress: 0.626,
  position: "P5",
  classCount: 18,
  gained: "+4",
  startPosition: "from P9",
  firstLapLabel: "L1",
  lastLapLabel: "L48",
  spread: "\u00b1 0.34s",
  lapCount: 48,
  pitStops: 2,
  lastPit: "last on L43",
  limits: 2,
  limitMax: 5,
  limitNote: "penalty at 5",
};
export function sampleValues(id, settings) {
  const result = JSON.parse(
    JSON.stringify({ ...common, ...samples[id] })
      .replaceAll("#FFD23F", settings.accent)
      .replaceAll("rgba(255,210,63,0.22)", `${settings.accent}38`),
  );
  if (id === "inputs" && !settings.showClutch) {
    result.pedals = result.pedals.filter((p) => p.label !== "C");
    result.clutchPts = "";
  }
  if (id === "leaderboard")
    result.classes = result.classes.map((cl) => ({
      ...cl,
      rows: cl.rows.slice(0, settings.rows),
    }));
  if (id === "relative") result.rows = result.rows.slice(0, settings.rows);
  if (id === "laps") result.laps = result.laps.slice(0, settings.rows);
  return result;
}
