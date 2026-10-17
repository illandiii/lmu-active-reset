const { FlatSpotHistory, flatSpotDetected } = require("./flatspot-model.cjs");
const { BrakeTrace, predictLock } = require("./brake-prediction.cjs");
const finite = Number.isFinite;
const names = ["FL", "FR", "RL", "RR"];
const keys = ["frontLeft", "frontRight", "rearLeft", "rearRight"];
const clamp = (n) => Math.max(0, Math.min(1, n));

function wheelSlip(w) {
  if (
    !w ||
    !w.attached ||
    !finite(w.load) ||
    w.load < 200 ||
    !finite(w.groundSpeed) ||
    !finite(w.rotation) ||
    !finite(w.radius) ||
    !finite(w.deflection) ||
    ![0, 1, 5].includes(w.surface)
  )
    return null;
  const speed = Math.abs(w.groundSpeed);
  const radius = w.radius - w.deflection;
  if (speed < 1 || radius < 0.15 || radius > 0.6) return null;
  // Use each wheel's ground speed, not chassis speed, to account for cornering.
  return {
    slip: clamp(1 - (Math.abs(w.rotation) * radius) / speed),
    speed: speed * 3.6,
  };
}

class WheelHistory {
  constructor() {
    this.events = [];
    this.braking = null;
    this.previous = null;
    this.born = [null, null, null, null];
    this.flatSpots = new FlatSpotHistory();
    this.brakeTrace = new BrakeTrace();
  }
  update(raw) {
    const p = raw.player;
    const car = raw.cars?.find((c) => c.id === p?.id);
    const previous = this.previous;
    const wheels = p?.wheels || [];
    if (!raw.driving || !finite(p?.brake) || p.brake <= 0.02)
      this.braking = null;
    else {
      const candidates = wheels
        .map(wheelSlip)
        .filter((w) => w && w.slip >= 0.05);
      const peak = candidates.sort((a, b) => b.slip - a.slip)[0];
      if (peak && (!this.braking || peak.slip > this.braking.slip)) {
        if (!this.braking) {
          this.braking = {};
          this.events.push(this.braking);
          if (this.events.length > 12) this.events.shift();
        }
        Object.assign(this.braking, peak, { brake: clamp(p.brake) });
      }
    }
    const replacements = [false, false, false, false];
    for (let i = 0; i < 4; i++) {
      const w = wheels[i],
        old = previous?.wheels?.[i];
      if (!w || !finite(p?.lap)) {
        this.born[i] = null;
        continue;
      }
      const replaced =
        old &&
        (w.compoundIndex !== old.compoundIndex ||
          (finite(w.wear) &&
            finite(old.wear) &&
            (w.wear - old.wear > 0.05 ||
              (car?.pit &&
                p.speed < 1 &&
                old.wear < 0.999 &&
                w.wear >= 0.999))));
      replacements[i] = Boolean(replaced);
      // Do not invent total tyre age when attaching halfway through a stint.
      if (
        replaced ||
        (this.born[i] === null && car?.pit && p.speed < 1 && w.wear >= 0.999)
      )
        this.born[i] = p.lap;
    }
    this.previous = p;
    const contacts = [0, 1, 2, 3].map((i) => {
      const w = wheels[i],
        slip = wheelSlip(w);
      if (
        !raw.driving ||
        !(p?.brake > 0.02) ||
        w?.flat ||
        !slip ||
        slip.slip < 0.8 ||
        slip.speed < 20 ||
        Math.abs(w.rotation) > 8
      )
        return null;
      return {
        sliding: (slip.speed / 3.6) * slip.slip,
        rotation: Math.abs(w.rotation),
        loadFactor: Math.max(0.25, Math.min(3, w.load / 3000)),
      };
    });
    const brakeTrace = this.brakeTrace.update(
      raw,
      wheels.map(wheelSlip),
      replacements,
    );
    return {
      brakeTrace,
      brakeModels: this.brakeTrace.models.map((m) => (m ? { ...m } : null)),
      flatSpots: this.flatSpots.update(p?.et, contacts, replacements),
      brakeEvents: this.events.map((e) => ({ ...e })),
      tyreLaps: this.born.map((lap) =>
        lap === null ? null : Math.max(0, p.lap - lap),
      ),
    };
  }
}

function brakeValues(data, settings) {
  const p = data?.connected ? data.player : null;
  const threshold = (settings.slipThreshold ?? 20) / 100;
  const minimumSpeed = settings.minimumSpeed ?? 20;
  const events = (data?.connected ? data.brakeEvents || [] : []).filter(
    (e) => e.slip >= threshold && e.speed >= minimumSpeed,
  );
  const brake = finite(p?.brake) ? clamp(p.brake) : 0;
  const predictions = names.map((_, i) =>
    p ? predictLock(data, settings, i, wheelSlip(p.wheels?.[i])) : null,
  );
  const locked = predictions.map((p) => Boolean(p?.locked));
  const limiting = predictions
    .map((p, index) => ({ ...p, index }))
    .filter((p) => finite(p.threshold))
    .sort((a, b) => a.threshold - b.threshold)[0];
  const earliest = predictions
    .filter((p) => p?.locked || p?.predicted)
    .sort((a, b) => a.eta - b.eta)[0];
  const observed = limiting?.threshold ?? null;
  const upcoming = predictions
    .map((p, i) =>
      (p?.locked || p?.predicted) && p.eta <= earliest.eta + 0.05
        ? names[i]
        : null,
    )
    .filter(Boolean)
    .join("/");
  return {
    brake,
    brakePct: finite(p?.brake) ? Math.round(brake * 100) : "-",
    threshold:
      observed === null
        ? "-"
        : observed > 1
          ? ">100"
          : `~${Math.round(observed * 100)}`,
    hasThreshold: observed !== null,
    thresholdOpacity: observed === null ? 0 : 1,
    thrLeft: `${clamp(observed ?? 1) * 100}%`,
    zoneW: `${(1 - clamp(observed ?? 1)) * 100}%`,
    thresholdColor: locked.some(Boolean)
      ? "#FF8A80"
      : earliest
        ? "#FFC062"
        : "#F2F4F6",
    fillColor: locked.some(Boolean)
      ? "#E2483D"
      : earliest
        ? "#F5A524"
        : "#6B7A8C",
    events: events.map((e) => ({ left: `${e.brake * 100}%` })),
    forecast: earliest
      ? `${upcoming} ${earliest.locked ? "LOCK" : earliest.eta === 0 ? "RISK" : `~${Math.round(earliest.eta * 1000)} ms`}`
      : limiting
        ? `${names[limiting.index]} limit`
        : "-",
    forecastColor: earliest?.locked ? "#FF8A80" : "#FFC062",
    tyres: locked.map((on, i) => ({
      name: names[i],
      locked: on,
      predicted: Boolean(predictions[i]?.predicted),
      title: predictions[i]
        ? `${names[i]}: ${on ? "locking" : finite(predictions[i].threshold) ? `estimated brake limit ${Math.round(predictions[i].threshold * 100)}%` : "-"}`
        : names[i],
      bg: on
        ? "#FF5A4E"
        : predictions[i]?.predicted
          ? "#F5A524"
          : "rgba(255,255,255,0.08)",
      border: on
        ? "#FFB3AC"
        : predictions[i]?.predicted
          ? "#FFC062"
          : "rgba(255,255,255,0.25)",
    })),
  };
}

function tyreValues(data, settings) {
  const p = data?.connected ? data.player : null;
  const cold = settings.coldTemp ?? 80,
    hot = settings.hotTemp ?? 105;
  const heat = (c) =>
    !finite(c)
      ? "#343B45"
      : c < cold - 10
        ? "#4C8DFF"
        : c < cold
          ? "#5EC8E8"
          : c < hot - 10
            ? "#3DDC84"
            : c < hot
              ? "#F5A524"
              : "#E2483D";
  const fg = (c) =>
    !finite(c)
      ? "#C2C9D0"
      : c < cold
        ? "#8FBBFF"
        : c < hot - 10
          ? "#7EE2A8"
          : c < hot
            ? "#FFC062"
            : "#FF8A80";
  const compounds = {
    S: ["Soft", "#F2F4F6"],
    M: ["Medium", "#F5C518"],
    H: ["Hard", "#E5322F"],
    W: ["Wet", "#2D9CDB"],
  };
  return {
    tyres: names.map((name, i) => {
      const w = p?.wheels?.[i];
      const temperatures = [0, 1, 2].map((j) => {
        const v = w?.temperatures?.[j];
        return finite(v) && v > -100 && v < 500 ? v : null;
      });
      const avg = temperatures.every(finite)
        ? temperatures.reduce((a, b) => a + b, 0) / 3
        : null;
      const life =
        finite(w?.wear) && w.wear >= 0 && w.wear <= 1 ? w.wear : null;
      const compoundName =
        (i < 2 ? p?.frontCompound : p?.rearCompound) || "Unavailable";
      const code = /soft/i.test(compoundName)
        ? "S"
        : /medium/i.test(compoundName)
          ? "M"
          : /hard/i.test(compoundName)
            ? "H"
            : /wet|rain/i.test(compoundName)
              ? "W"
              : "-";
      const flat = w?.flat === true;
      const estimatedFlat = Boolean(
        p &&
        !flat &&
        flatSpotDetected(data.flatSpots?.[i], settings.flatSpotSensitivity),
      );
      return {
        key: keys[i],
        name,
        avg: avg === null ? "-" : Math.round(avg),
        c0: heat(temperatures[0]),
        c1: heat(temperatures[1]),
        c2: heat(temperatures[2]),
        tempFg: fg(avg),
        life: life ?? 0,
        lifePct: life === null ? "-" : Math.round(life * 100),
        lifeFg:
          life === null
            ? "#C2C9D0"
            : life < 0.3
              ? "#FF8A80"
              : life < 0.5
                ? "#FFC062"
                : "#F2F4F6",
        compound: code,
        compoundName,
        cc: compounds[code]?.[1] || "#677383",
        flat,
        estimatedFlat,
        marked: flat || estimatedFlat,
        conditionText: flat ? "puncture" : estimatedFlat ? "~flat spot" : "",
        conditionTitle: flat
          ? "Puncture reported by LMU"
          : "Estimated from observed wheel locks",
        laps: data?.connected ? (data.tyreLaps?.[i] ?? "-") : "-",
      };
    }),
  };
}

module.exports = { WheelHistory, wheelSlip, brakeValues, tyreValues };
