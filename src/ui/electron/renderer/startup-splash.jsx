import React, { useEffect, useState } from "react";
import logo from "@assets/sector-logo.png";
import { StartupWord } from "./startup-word.jsx";
import "./startup-splash.css";

export function StartupSplash({ reduceMotion, onDone }) {
  const [leaving, setLeaving] = useState(false);
  useEffect(() => {
    let done = false;
    let exitTimer;
    const media = matchMedia("(prefers-reduced-motion: reduce)");
    const finish = (event) => {
      // A skip must not activate the control revealed beneath the splash.
      if (event) {
        event.preventDefault();
        event.stopImmediatePropagation();
      }
      if (done) return;
      done = true;
      clearTimeout(holdTimer);
      setLeaving(true);
      exitTimer = setTimeout(onDone, 420);
    };
    const holdTimer = setTimeout(
      finish,
      reduceMotion || media.matches ? 700 : 4100,
    );
    const motionChanged = () => {
      if (media.matches) finish();
    };
    window.addEventListener("keydown", finish, true);
    window.addEventListener("pointerdown", finish, true);
    media.addEventListener("change", motionChanged);
    return () => {
      clearTimeout(holdTimer);
      clearTimeout(exitTimer);
      window.removeEventListener("keydown", finish, true);
      window.removeEventListener("pointerdown", finish, true);
      media.removeEventListener("change", motionChanged);
    };
  }, [reduceMotion, onDone]);

  return (
    <div
      className={`splash${leaving ? " is-leaving" : ""}`}
      id="sector-splash"
      role="img"
      aria-label="Sector is starting"
    >
      <div className="splash-card">
        <img className="splash-logo" src={logo} alt="" draggable="false" />
        <StartupWord />
      </div>
    </div>
  );
}
