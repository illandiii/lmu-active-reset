const finite = Number.isFinite;
const radians = (degrees) => (degrees * Math.PI) / 180;
function slipAngle(w) {
  return finite(w?.lateralSpeed) &&
    finite(w?.groundSpeed) &&
    Math.abs(w.groundSpeed) >= 1
    ? (Math.atan2(w.lateralSpeed, Math.abs(w.groundSpeed)) * 180) / Math.PI
    : null;
}

function regression(samples, xKey, yKey) {
  const x = samples.reduce((sum, s) => sum + s[xKey], 0) / samples.length;
  const y = samples.reduce((sum, s) => sum + s[yKey], 0) / samples.length;
  let xx = 0,
    xy = 0,
    yy = 0;
  for (const s of samples) {
    xx += (s[xKey] - x) ** 2;
    xy += (s[xKey] - x) * (s[yKey] - y);
    yy += (s[yKey] - y) ** 2;
  }
  const slope = xx ? xy / xx : 0;
  return {
    slope,
    intercept: y - slope * x,
    fit: xx && yy ? (xy * xy) / (xx * yy) : 0,
  };
}

function comparable(sample, current) {
  return (
    Math.abs(sample.speed - current.speed) <=
      Math.max(8, current.speed * 0.2) &&
    sample.load / current.load >= 0.7 &&
    sample.load / current.load <= 1.3 &&
    Math.abs((sample.steering ?? 0) - (current.steering ?? 0)) <= 0.25 &&
    Math.abs(sample.angle - current.angle) <= 15
  );
}

function fitResponse(samples, current) {
  const selected = samples.filter(
    (s) =>
      current.time - s.time >= 0 &&
      current.time - s.time <= 3 &&
      comparable(s, current) &&
      s.slip < 0.3,
  );
  if (
    selected.length < 4 ||
    selected.at(-1).time - selected[0].time < 0.1 ||
    Math.max(...selected.map((s) => s.brake)) -
      Math.min(...selected.map((s) => s.brake)) <
      0.08
  )
    return null;
  const fit = regression(selected, "brake", "slip");
  if (fit.slope < 0.05 || fit.fit < 0.7) return null;
  return { ...current, ...fit };
}

class BrakeTrace {
  constructor() {
    this.wheels = [[], [], [], []];
    this.models = [null, null, null, null];
    this.lastTime = null;
  }
  update(raw, slips, replacements = []) {
    const p = raw.player;
    const dt = p?.et - this.lastTime;
    const reset =
      !raw.driving ||
      !finite(p?.et) ||
      (this.lastTime !== null && (dt < 0 || dt > 0.25));
    this.lastTime = finite(p?.et) ? p.et : null;
    this.wheels = this.wheels.map((samples, i) => {
      const w = p?.wheels?.[i],
        slip = slips[i],
        angle = slipAngle(w);
      if (reset || replacements[i] || !slip || angle === null) {
        this.models[i] = null;
        return [];
      }
      samples = samples.filter((s) => p.et - s.time <= 3);
      if (!finite(p.brake) || p.brake <= 0.05 || samples.at(-1)?.time === p.et)
        return samples;
      const current = {
        time: p.et,
        brake: p.brake,
        speed: slip.speed,
        slip: slip.slip,
        angle,
        load: w.load,
        steering: finite(p.steering) ? p.steering : 0,
      };
      samples = [...samples, current].slice(-120);
      const learned = fitResponse(samples, current);
      if (learned) this.models[i] = learned;
      return samples;
    });
    return this.wheels;
  }
}

function predictLock(data, settings, index, slip) {
  const p = data?.player,
    wheel = p?.wheels?.[index];
  if (
    !data?.driving ||
    !p ||
    !slip ||
    slip.speed < (settings.minimumSpeed ?? 20)
  )
    return null;
  const angle = slipAngle(wheel),
    goal = (settings.slipThreshold ?? 20) / 100;
  const locked = p.brake > 0.02 && slip.slip >= goal;
  if (angle === null)
    return locked
      ? { locked, predicted: false, threshold: null, eta: 0 }
      : null;
  const current = {
    time: p.et,
    speed: slip.speed,
    load: wheel.load,
    angle,
    steering: finite(p.steering) ? p.steering : 0,
  };
  const samples = data.brakeTrace?.[index] || [];
  const learned = fitResponse(samples, current) || data.brakeModels?.[index];
  const valid =
    learned &&
    p.et >= learned.time &&
    p.et - learned.time <= 12 &&
    comparable(learned, current) &&
    (locked ||
      !(p.brake > 0.05) ||
      Math.abs(learned.slope * p.brake + learned.intercept - slip.slip) <=
        0.08);
  if (!valid)
    return locked
      ? { locked, predicted: false, threshold: null, eta: 0, angle }
      : null;

  // Invert the observed pedal/slip response at the current lateral demand.
  // This is an estimated warning threshold, not the game's hidden tyre model.
  const lateral = Math.tan(radians(Math.min(80, Math.abs(angle))));
  const remainingSlip = Math.sqrt(Math.max(0, goal * goal - lateral * lateral));
  const threshold = Math.max(
    0,
    (remainingSlip - learned.intercept) / learned.slope,
  );
  const recent = samples.filter(
    (s) => p.et - s.time >= 0 && p.et - s.time <= 0.4,
  );
  const pedalRate =
    recent.length >= 4 && p.et - recent.at(-1).time <= 0.1
      ? regression(recent, "time", "brake").slope
      : 0;
  const braking = p.brake > 0.02;
  const eta =
    locked || (braking && p.brake >= threshold)
      ? 0
      : braking && pedalRate > 0.03
        ? (threshold - p.brake) / pedalRate
        : null;
  const predicted =
    !locked &&
    braking &&
    threshold <= 1 &&
    (p.brake >= threshold ||
      (eta !== null &&
        eta > 0 &&
        eta <= (settings.predictionMs ?? 500) / 1000));
  return { locked, predicted, threshold, eta, angle };
}
module.exports = { BrakeTrace, predictLock, slipAngle };
