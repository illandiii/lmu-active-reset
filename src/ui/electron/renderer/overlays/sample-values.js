import samples from "./samples.json";
import wheels from "../../shared/wheel-model.cjs";
import stint from "../../shared/stint-model.cjs";
import hybrid from "../../shared/hybrid-model.cjs";
import compounds from "../../shared/compound-model.cjs";
const common = {
  sessionName: "RACE",
  trackCars: "Spa-Francorchamps, 56 cars",
  lap: 48,
  remaining: "2:14:37",
  elapsed: "3:45:23",
  best: "2:18.412",
  surface: "Dry track, grip 98%",
  air: "17\u00b0",
  trackTemp: "24\u00b0",
  humidity: "68%",
  forecast: "Rain likely in ~1h 20m",
  rainLap: "\u2248 lap 82",
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
  sessionTrack: "6 Hours Race, Spa-Francorchamps",
  classBadge: "GT3",
  classColor: "#1FA34A",
  progress: 0.626,
  position: "P5",
  classCount: 18,
  gained: "+4",
  startPosition: "from P9",
  firstLapLabel: "L1",
  lastLapLabel: "L48",
  lapCount: 48,
  pitStops: 2,
  lastPit: "last on L43",
  limits: 2,
  limitMax: 5,
};
export function sampleValues(id, settings) {
  if (id === "hybrid")
    return hybrid.hybridValues(
      {
        connected: true,
        player: {
          hybrid: {
            state: 2,
            torque: 172,
            rpm: 10000,
            battery: 0.45,
            map: 4,
            mapMax: 5,
          },
        },
        hybridLaps: [
          { map: 4, delta: -1.4, seconds: 123 },
          { map: 4, delta: -1.3, seconds: 124 },
          { map: 3, delta: 1, seconds: 124 },
          { map: 3, delta: 1.1, seconds: 125 },
        ],
      },
      settings,
    );
  if (["brake-lock", "tyres", "stint-planner"].includes(id)) {
    const data = {
      connected: true,
      driving: true,
      player: {
        id: 1,
        et: 0.3,
        brake: 0.64,
        energy: 0.412,
        fuel: 38.6,
        frontCompound: "Soft",
        rearCompound: "Medium",
        wheels: [0.71, 0.78, 0.84, 0.86].map((wear, i) => ({
          attached: true,
          load: 3000,
          groundSpeed: 30,
          rotation: i === 0 ? 86 : 100,
          lateralSpeed: 2,
          radius: 0.31,
          deflection: 0.01,
          surface: 0,
          wear,
          flat: false,
          temperatures: [
            [98, 93, 88],
            [91, 88, 85],
            [86, 84, 82],
            [84, 83, 81],
          ][i],
        })),
      },
      cars: [{ id: 1, laps: 48 }],
      tyreLaps: [6, 6, 18, 18],
      flatSpots: [{ exposure: 60, duration: 2 }, null, null, null],
      brakeTrace: [
        Array.from({ length: 7 }, (_, i) => ({
          time: i * 0.05,
          slip: 0.05 + i * 0.015,
          brake: 0.4 + i * 0.04,
          speed: 108,
          angle: (Math.atan2(2, 30) * 180) / Math.PI,
          load: 3000,
        })),
        [],
        [],
        [],
      ],
      brakeEvents: [0.81, 0.86, 0.79, 0.92, 0.84, 0.8].map((brake) => ({
        brake,
        slip: 0.6,
        speed: 108,
      })),
      laps: [{ clean: true, energy: 3.6, fuel: 2.42 }],
    };
    return id === "brake-lock"
      ? wheels.brakeValues(data, settings)
      : id === "tyres"
        ? wheels.tyreValues(data, settings)
        : stint.stintValues(data, settings);
  }
  const result = JSON.parse(
    JSON.stringify({ ...common, ...samples[id] })
      .replaceAll("#FFD23F", settings.accent)
      .replaceAll("rgba(255,210,63,0.22)", `${settings.accent}38`),
  );
  if (id === "inputs" && !settings.showClutch) {
    result.pedals = result.pedals.filter((p) => p.label !== "C");
    result.clutchPts = "";
  }
  if (id === "inputs")
    result.pedals = result.pedals.filter(
      (p) =>
        settings.columns?.[
          p.label === "T" ? "throttle" : p.label === "B" ? "brake" : "clutch"
        ] !== false,
    );
  if (id === "leaderboard")
    result.classes = result.classes.map((cl) => ({
      ...cl,
      rows: cl.rows.slice(0, settings.rows).map((r, i) => ({
        ...r,
        ...compounds.resourceValues({
          compounds:
            i % 3 === 1
              ? ["Soft", "Soft", "Medium", "Medium"]
              : ["Medium", "Medium", "Medium", "Medium"],
          energy: ["GT3", "HY"].includes(cl.badge) ? (58 - i * 7) / 100 : null,
        }),
      })),
    }));
  if (id === "relative") result.rows = result.rows.slice(0, settings.rows);
  if (id === "laps") result.laps = result.laps.slice(0, settings.rows);
  return result;
}
