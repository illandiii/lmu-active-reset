import React, { useState, useEffect, useRef } from "react";
import layout from "../../shared/overlay-layout.cjs";
import { Widget } from "./widget.jsx";
import { sampleValues } from "./sample-values.js";
import model from "../../shared/overlay-model.cjs";
import { Icon } from "@iconify/react";
import lock from "@iconify-icons/solar/lock-keyhole-linear";
import move from "@iconify-icons/solar/move-square-linear";
import "./editor.css";
const api = window.sector;
function NumberField({
  label,
  value,
  min,
  max,
  step = 1,
  onCommit,
  suffix = "",
  live = false,
}) {
  const [draft, setDraft] = useState(String(value));
  const focused = useRef(false);
  useEffect(() => {
    if (!focused.current) setDraft(String(value));
  }, [value]);
  return (
    <label className="overlay-field">
      <span>{label}</span>
      <div>
        <input
          aria-label={label}
          type="number"
          value={draft}
          min={min}
          max={max}
          step={step}
          onFocus={() => {
            focused.current = true;
          }}
          onChange={(e) => {
            setDraft(e.target.value);
            const n = e.target.valueAsNumber;
            if (
              live &&
              Number.isFinite(n) &&
              n >= min &&
              n <= max &&
              n !== value
            )
              onCommit(n);
          }}
          onBlur={(e) => {
            focused.current = false;
            const n = e.target.valueAsNumber;
            if (Number.isFinite(n) && n >= min && n <= max) {
              if (n !== value) onCommit(n);
              setDraft(String(n));
            } else setDraft(String(value));
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") e.currentTarget.blur();
          }}
        />
        {suffix && <span>{suffix}</span>}
      </div>
    </label>
  );
}
export function OverlayEditor() {
  const [state, setState] = useState(null),
    [selected, setSelected] = useState("inputs"),
    [error, setError] = useState("");
  const [width, setWidth] = useState(600);
  const stage = useRef(null);
  useEffect(() => {
    let received = false,
      live = true;
    const off = api.onOverlays((value) => {
      received = true;
      if (live) setState(value);
    });
    api
      .initializeOverlays()
      .then((value) => {
        if (live && !received) setState(value);
      })
      .catch((e) => setError(e.message));
    return () => {
      live = false;
      off();
    };
  }, []);
  useEffect(() => {
    if (!stage.current) return;
    const observer = new ResizeObserver(([entry]) =>
      setWidth(entry.contentRect.width),
    );
    observer.observe(stage.current);
    return () => observer.disconnect();
  }, [Boolean(state)]);
  async function save(config) {
    const previous = state.config;
    setState((current) => ({ ...current, config }));
    try {
      const result = await api.saveOverlays(config);
      if (!result.ok) throw new Error(result.error);
      setState(result);
      setError("");
    } catch (e) {
      setState((current) => ({ ...current, config: previous }));
      setError(e.message);
    }
  }
  async function edit() {
    try {
      setState(await api.editOverlays(!state.editing));
    } catch (e) {
      setError(e.message);
    }
  }
  if (!state) return error ? <p role="alert">{error}</p> : null;
  const { config, definitions, frame } = state,
    setting = config.widgets[selected];
  const update = (patch) =>
    save({
      ...config,
      widgets: { ...config.widgets, [selected]: { ...setting, ...patch } },
    });

  const values = frame?.connected
    ? model.valuesFor(selected, frame, setting)
    : sampleValues(selected, setting);
  const def = layout.dimensions(selected, setting, values);
  const scale = Math.min(setting.scale, (width - 32) / def.width);
  return (
    <div className="overlay-editor" data-tour="overlay-editor">
      <div className="overlay-toolbar">
        <button
          role="switch"
          aria-label="Enable overlays"
          aria-checked={config.enabled}
          className="switch"
          onClick={() => save({ ...config, enabled: !config.enabled })}
        >
          <span />
        </button>
        <span>Overlays</span>
        <button
          className="secondary layout-toggle"
          aria-pressed={state.editing}
          onClick={edit}
        >
          <Icon icon={state.editing ? lock : move} />
          {state.editing ? "Lock layout" : "Edit layout"}
        </button>
      </div>
      <div className="overlay-workspace">
        <div className="widget-list" role="group" aria-label="Widgets">
          {Object.entries(definitions).map(([id, d]) => (
            <div key={id} className={id === selected ? "selected" : ""}>
              <button
                className="widget-choice"
                aria-pressed={id === selected}
                onClick={() => setSelected(id)}
              >
                {d.name}
              </button>
              <input
                type="checkbox"
                aria-label={`Show ${d.name}`}
                checked={config.widgets[id].enabled}
                onChange={(e) =>
                  save({
                    ...config,
                    widgets: {
                      ...config.widgets,
                      [id]: {
                        ...config.widgets[id],
                        enabled: e.target.checked,
                      },
                    },
                  })
                }
              />
            </div>
          ))}
        </div>
        <div className="widget-inspector">
          <div className="widget-stage" ref={stage}>
            {frame?.connected && <span className="stage-mode">Live</span>}
            <div
              style={{ width: def.width * scale, height: def.height * scale }}
            >
              <div
                style={{
                  width: def.width,
                  height: def.height,
                  transform: `scale(${scale})`,
                  transformOrigin: "top left",
                }}
              >
                <Widget id={selected} values={values} settings={setting} />
              </div>
            </div>
          </div>
          <div className="overlay-parts" aria-label="Widget columns">
            {layout.columns[selected].map(([key, label]) => (
              <label key={key}>
                <input
                  type="checkbox"
                  aria-label={`Column ${label}`}
                  checked={layout.visible(setting, key)}
                  onChange={(e) =>
                    update({
                      columns: { ...setting.columns, [key]: e.target.checked },
                      ...(key === "logo"
                        ? { showLogos: e.target.checked }
                        : {}),
                    })
                  }
                />
                {label}
              </label>
            ))}
            {(selected === "inputs"
              ? [
                  ["showClutch", "Clutch"],
                  ["showTrace", "Trace"],
                  ["showWheel", "Steering wheel"],
                  ["showGear", "Gear / speed"],
                ]
              : []
            ).map(([key, label]) => (
              <label key={key}>
                <input
                  type="checkbox"
                  checked={setting[key]}
                  onChange={(e) => update({ [key]: e.target.checked })}
                />
                {label}
              </label>
            ))}
          </div>
          <div className="overlay-controls">
            <NumberField
              label="X"
              value={setting.x}
              min={0}
              max={20000}
              onCommit={(x) => update({ x })}
              suffix="px"
            />
            <NumberField
              label="Y"
              value={setting.y}
              min={0}
              max={20000}
              onCommit={(y) => update({ y })}
              suffix="px"
            />
            <NumberField
              label="Size"
              live
              value={Math.round(setting.scale * 100)}
              min={50}
              max={200}
              step={5}
              onCommit={(scale) => update({ scale: scale / 100 })}
              suffix="%"
            />
            <NumberField
              label="Opacity"
              value={Math.round(setting.opacity * 100)}
              min={0}
              max={100}
              step={5}
              onCommit={(opacity) => update({ opacity: opacity / 100 })}
              suffix="%"
            />
            {((selected === "inputs" && setting.showWheel) ||
              (selected === "session-summary" &&
                layout.visible(setting, "position"))) && (
              <label className="overlay-field">
                <span>Accent</span>
                <input
                  type="color"
                  aria-label="Accent"
                  value={setting.accent}
                  onChange={(e) => update({ accent: e.target.value })}
                />
              </label>
            )}
            {["leaderboard", "relative", "laps"].includes(selected) && (
              <NumberField
                label="Rows"
                value={setting.rows}
                min={3}
                max={selected === "laps" ? 8 : selected === "relative" ? 7 : 12}
                onCommit={(rows) => update({ rows })}
              />
            )}
            {selected === "inputs" && (
              <NumberField
                label="Steering rotation"
                value={setting.lockDeg}
                min={90}
                max={540}
                step={10}
                onCommit={(lockDeg) => update({ lockDeg })}
                suffix="°"
              />
            )}
            {(selected === "brake-lock"
              ? [
                  ["slipThreshold", "Slip threshold", 5, 90, "%"],
                  ["minimumSpeed", "Minimum speed", 5, 150, "km/h"],
                  ["predictionMs", "Prediction window", 100, 1000, "ms"],
                ]
              : selected === "tyres"
                ? [
                    ["coldTemp", "Cold below", 0, 150, "°C"],
                    ["hotTemp", "Hot above", 20, 200, "°C"],
                    [
                      "flatSpotSensitivity",
                      "Flat spot sensitivity",
                      10,
                      100,
                      "%",
                    ],
                  ]
                : selected === "stint-planner"
                  ? [
                      ["consumptionLaps", "Consumption average", 1, 20, "laps"],
                      ["reserveLaps", "Reserve", 0, 5, "laps"],
                    ]
                  : selected === "hybrid"
                    ? [
                        ["targetSoc", "Battery target", 10, 90, "%"],
                        ["targetLaps", "Target horizon", 1, 10, "laps"],
                        ["powerScale", "Power bar scale", 50, 500, "kW"],
                      ]
                    : []
            ).map(([key, label, min, max, suffix]) => (
              <NumberField
                key={key}
                label={label}
                value={setting[key]}
                min={min}
                max={max}
                suffix={suffix}
                onCommit={(value) => update({ [key]: value })}
              />
            ))}
          </div>
        </div>
      </div>
      {(error || state.error) && (
        <p role="alert" className="overlay-error">
          {error || state.error}
        </p>
      )}
    </div>
  );
}
