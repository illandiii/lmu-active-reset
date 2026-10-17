import React, { useEffect, useRef, useState } from "react";
import { Icon } from "@iconify/react";
import play from "@iconify-icons/solar/play-linear";
import check from "@iconify-icons/solar/check-circle-linear";
import layout from "../shared/overlay-layout.cjs";
import model from "../shared/overlay-model.cjs";
import "./home.css";

const api = window.sector;

function LayoutPreview({ state }) {
  const { config, displays, definitions, frame } = state;
  const display =
    displays.find((d) => d.id === config.displayId) ||
    displays.find((d) => d.primary) ||
    displays[0];
  if (!display) return null;
  const { width, height } = display.bounds;
  return (
    <div
      className="home-layout"
      role="img"
      aria-label="Current overlay layout"
      style={{
        aspectRatio: `${width} / ${height}`,
        "--display-ratio": width / height,
      }}
    >
      {Object.entries(config.widgets)
        .filter(([, w]) => w.enabled)
        .map(([id, w]) => {
          const values = frame?.connected
            ? model.valuesFor(id, frame, w)
            : undefined;
          const size = layout.dimensions(id, w, values);
          const scale = Math.min(
            w.scale,
            width / size.width,
            height / size.height,
          );
          const widgetWidth = Math.round(size.width * scale);
          const widgetHeight = Math.round(size.height * scale);
          return (
            <span
              key={id}
              title={definitions[id].name}
              style={{
                left: `${(Math.max(0, Math.min(w.x, width - widgetWidth)) / width) * 100}%`,
                top: `${(Math.max(0, Math.min(w.y, height - widgetHeight)) / height) * 100}%`,
                width: `${(widgetWidth / width) * 100}%`,
                height: `${(widgetHeight / height) * 100}%`,
              }}
            />
          );
        })}
    </div>
  );
}

export function Home({
  game,
  preview,
  view,
  status,
  saveKey,
  restoreKey,
  disabled,
  onNavigate,
}) {
  const [state, setState] = useState(null);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState({});
  const inFlight = useRef(new Set());
  useEffect(() => {
    let live = true,
      received = false;
    const off = api.onOverlays((value) => {
      received = true;
      if (live) setState(value);
    });
    api
      .initializeOverlays()
      .then((value) => {
        if (live && !received) setState(value);
      })
      .catch((e) => {
        if (live) setError(e.message);
      });
    return () => {
      live = false;
      off();
    };
  }, []);

  async function action(name, callback) {
    if (inFlight.current.has(name)) return;
    inFlight.current.add(name);
    setBusy((current) => ({ ...current, [name]: true }));
    setError("");
    try {
      const result = await callback();
      if (result?.ok === false) throw new Error(result.error);
    } catch (e) {
      setError(e.message);
    } finally {
      inFlight.current.delete(name);
      setBusy((current) => ({ ...current, [name]: false }));
    }
  }

  const frame = state?.frame?.connected ? state.frame : null;
  const car = frame?.cars?.find((c) =>
    frame.player ? c.id === frame.player.id : c.player,
  );
  const inSession = Boolean(frame?.cars?.length);
  const elapsed =
    Number.isFinite(frame?.et) && frame.et >= 0
      ? [
          Math.floor(frame.et / 3600),
          Math.floor(frame.et / 60) % 60,
          Math.floor(frame.et) % 60,
        ]
          .map((part, index) =>
            index ? String(part).padStart(2, "0") : String(part),
          )
          .join(":")
      : null;
  const sessionDetails = [car?.vehicle, elapsed ? `session ${elapsed}` : null]
    .filter(Boolean)
    .join(", ");
  const running = game.running || inSession;
  const enabledCount = state
    ? Object.values(state.config.widgets).filter((w) => w.enabled).length
    : 0;
  const total = state ? Object.keys(state.definitions).length : 0;
  const resetBlocked = disabled || busy.reset || status.phase !== "active";
  return (
    <div className="home-content">
      <div className="home-heading">
        <h1>
          Home<span className="accent">.</span>
        </h1>
        <button
          className={`home-launch ${running ? "is-running" : ""}`}
          disabled={
            disabled || busy.launch || preview || running || game.launching
          }
          title={preview ? "Game launch is unavailable in preview" : undefined}
          onClick={() => action("launch", () => api.launchGame())}
        >
          <Icon icon={running ? check : play} aria-hidden="true" />
          {running
            ? "LMU running"
            : game.launching
              ? "Launching LMU..."
              : "Launch LMU"}
        </button>
      </div>

      <section className="home-session" aria-label="Session overview">
        <div className="home-session-info">
          <span
            className={
              inSession ? "home-session-state is-live" : "home-session-state"
            }
          >
            {inSession
              ? status.phase === "active"
                ? "In session"
                : "Session loaded"
              : running
                ? "LMU running"
                : game.launching
                  ? "Launching LMU..."
                  : "LMU isn't running"}
          </span>
          <h2>
            {inSession
              ? frame.track || "Session"
              : running
                ? "Waiting for session"
                : "Ready when you are"}
          </h2>
          {sessionDetails && <p>{sessionDetails}</p>}
        </div>
        <dl className="home-stats">
          <div>
            <dt>Laps</dt>
            <dd>{car?.laps ?? "-"}</dd>
          </div>
          <div>
            <dt>Best</dt>
            <dd className="home-best">{model.time(car?.best)}</dd>
          </div>
          <div>
            <dt>Resets</dt>
            <dd>{view.count}</dd>
          </div>
        </dl>
      </section>

      <div className="home-cards">
        <section className="home-card" aria-label="Active Reset overview">
          <h2>Active Reset</h2>
          <div className="home-keys">
            <button
              className="home-key"
              disabled={resetBlocked}
              onClick={() => action("reset", () => api.reset("save"))}
              aria-label="Save checkpoint"
            >
              <kbd>{saveKey}</kbd>
              <span>Save checkpoint</span>
            </button>
            <button
              className="home-key"
              disabled={resetBlocked || !view.saved}
              onClick={() => action("reset", () => api.reset("restore"))}
              aria-label="Restore checkpoint"
            >
              <kbd>{restoreKey}</kbd>
              <span>Restore checkpoint</span>
            </button>
          </div>
          <div
            className={`home-checkpoint ${view.saved ? "is-saved" : ""}`}
            role="status"
          >
            {view.saved && <Icon icon={check} aria-hidden="true" />}
            {view.saved ? "Checkpoint saved" : "No checkpoint saved"}
          </div>
          <button
            className="home-button home-open-reset"
            onClick={() => onNavigate("reset")}
          >
            Open Active Reset
          </button>
        </section>

        <section className="home-card" aria-label="Overlay overview">
          <div className="home-card-heading">
            <h2>Overlay</h2>
            {state && (
              <span className="home-badge">
                {enabledCount} of {total} selected,{" "}
                {state.editing ? "Editing" : "Locked"}
              </span>
            )}
          </div>
          {state && <LayoutPreview state={state} />}
          <div className="home-overlay-actions">
            <button
              className="home-button"
              disabled={disabled || busy.layout || !state}
              onClick={() =>
                action("layout", async () => {
                  await api.editOverlays(true);
                  onNavigate("overlay");
                })
              }
            >
              Edit layout
            </button>
            <button
              className="home-button home-overlay-toggle"
              role="switch"
              aria-label="Enable overlays"
              aria-checked={state?.config.enabled || false}
              disabled={disabled || !state}
              aria-busy={Boolean(busy.overlays)}
              onClick={() =>
                action("overlays", () =>
                  api.saveOverlays({
                    ...state.config,
                    enabled: !state.config.enabled,
                  }),
                )
              }
            >
              Overlays {state?.config.enabled ? "on" : "off"}
            </button>
          </div>
        </section>
      </div>
      {(error || game.error) && (
        <p className="home-error" role="alert">
          {error || game.error}
        </p>
      )}
    </div>
  );
}
