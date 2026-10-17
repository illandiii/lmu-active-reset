import React from "react";
import { createRoot } from "react-dom/client";
import { flushSync } from "react-dom";
import { Widget } from "../../renderer/overlays/widget.jsx";
import { sampleValues } from "../../renderer/overlays/sample-values.js";
import "./gallery.css";

const root = createRoot(document.getElementById("root"));
const names = {
  leaderboard: "Leaderboard",
  relative: "Relative",
  weather: "Weather",
  delta: "Delta",
  inputs: "Inputs",
  laps: "Laps",
  "session-summary": "Session",
  "brake-lock": "Brake lock",
  tyres: "Tyres",
  "stint-planner": "Stint planner",
};

// A deterministic, explicitly labelled demonstration, using the production widgets.
// No game process, telemetry reader or reset host is involved in these recordings.
function valuesAt(id, settings, seconds) {
  const values = sampleValues(id, settings);
  const cycle = (t) => ((t % 6) + 6) % 6;
  const throttle = (t) => {
    const phase = cycle(t);
    return phase < 2 ? 0.92 : phase < 3 ? 0 : Math.min(1, (phase - 3) / 2);
  };
  const brake = (t) => {
    const phase = cycle(t);
    return phase >= 2 && phase < 3.5 ? 0.85 * (1 - (phase - 2) / 1.5) : 0;
  };
  const trace = (input) =>
    Array.from({ length: 90 }, (_, i) => {
      const x = (i / 89) * 214;
      return `${x.toFixed(1)},${(123 - 120 * input(seconds - 6 + (i / 89) * 6)).toFixed(1)}`;
    }).join(" ");
  if (id === "inputs") {
    values.pedals = values.pedals.map((pedal) => {
      const v =
        pedal.label === "T"
          ? throttle(seconds)
          : pedal.label === "B"
            ? brake(seconds)
            : 0;
      return { ...pedal, v, pct: Math.round(v * 100) };
    });
    values.throttlePts = trace(throttle);
    values.brakePts = trace(brake);
    values.clutchPts = trace(() => 0);
    values.angle = (60 * Math.sin((seconds / 6) * Math.PI * 2)).toFixed(1);
    values.speed = Math.round(150 + 35 * Math.cos((seconds / 6) * Math.PI * 2));
    values.gear = values.speed > 165 ? 5 : values.speed > 130 ? 4 : 3;
  }
  if (id === "delta") {
    const delta = 0.34 * Math.sin((seconds / 6) * Math.PI * 2);
    values.deltaTxt = `${delta < 0 ? "−" : "+"}${Math.abs(delta).toFixed(3)}`;
    values.deltaFg = delta < 0 ? "#3DDC84" : "#FF4D4D";
    values.fastScale = Math.max(0, -delta);
    values.slowScale = Math.max(0, delta);
    values.pred = `2:${(18.412 + delta).toFixed(3)}`;
  }
  if (id === "weather") {
    // Do not illustrate forecast capabilities that the telemetry reader does not provide.
    values.surface = "Dry track";
    values.humidity = "-";
    values.forecast = "-";
    values.rainLap = "";
    values.slots = values.slots.map((slot, index) =>
      index === 0
        ? slot
        : {
            ...slot,
            temp: "-",
            rain: "-",
            rainScale: 0,
            isRain: false,
            isCloud: false,
            isPartly: false,
            isSun: false,
          },
    );
  }
  return values;
}

window.renderSectorMedia = (seconds, settings, animated = false) => {
  const columns = animated
    ? [["inputs", "delta"]]
    : [
        ["leaderboard", "weather", "brake-lock"],
        ["inputs", "laps", "delta", "stint-planner"],
        ["relative", "session-summary", "tyres"],
      ];
  flushSync(() =>
    root.render(
      <main id="showcase" className={animated ? "animated" : "gallery"}>
        <header>
          <strong>Sector overlays</strong>
          <span>DEMO DATA</span>
        </header>
        <div className="columns">
          {columns.map((column, index) => (
            <div className="column" key={index}>
              {column.map((id) => (
                <section key={id}>
                  <h2>{names[id]}</h2>
                  <Widget
                    id={id}
                    settings={settings[id]}
                    values={valuesAt(id, settings[id], seconds)}
                  />
                </section>
              ))}
            </div>
          ))}
        </div>
      </main>,
    ),
  );
};
