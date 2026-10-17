const finite = Number.isFinite;
// A combined-slip warning metric, not a calibrated friction coefficient.
// Lateral demand advances the warning; only longitudinal slip confirms a lock.
const combinedSlip = (slip, angle) =>
  Math.hypot(slip, Math.tan((Math.min(80, Math.abs(angle)) * Math.PI) / 180));
function slipAngle(w) {
  return finite(w?.lateralSpeed) &&
    finite(w?.groundSpeed) &&
    Math.abs(w.groundSpeed) >= 1
    ? (Math.atan2(w.lateralSpeed, Math.abs(w.groundSpeed)) * 180) / Math.PI
    : null;
}

class BrakeTrace {
  constructor() {
    this.wheels = [[], [], [], []];
  }
  update(raw, slips) {
    const p = raw.player;
    this.wheels = this.wheels.map((samples, i) => {
      const w = p?.wheels?.[i],
        slip = slips[i],
        angle = slipAngle(w);
      if (
        !raw.driving ||
        !finite(p?.et) ||
        !finite(p.brake) ||
        p.brake <= 0.02 ||
        !slip ||
        slip.speed < 5 ||
        angle === null
      )
        return [];
      const last = samples.at(-1);
      const dt = p.et - last?.time;
      if (dt === 0) return samples;
      if (last && (dt < 0 || dt > 0.15)) samples = [];
      return [
        ...samples.filter((s) => p.et - s.time <= 0.4),
        {
          time: p.et,
          brake: p.brake,
          speed: slip.speed,
          slip: slip.slip,
          angle,
          load: w.load,
        },
      ].slice(-16);
    });
    return this.wheels;
  }
}

function regression(samples, key) {
  const origin = samples[0].time;
  const meanX =
    samples.reduce((s, p) => s + p.time - origin, 0) / samples.length;
  const meanY = samples.reduce((s, p) => s + p[key], 0) / samples.length;
  let xx = 0,
    xy = 0,
    yy = 0;
  for (const p of samples) {
    const x = p.time - origin - meanX,
      y = p[key] - meanY;
    xx += x * x;
    xy += x * y;
    yy += y * y;
  }
  return { slope: xx ? xy / xx : 0, fit: xx && yy ? (xy * xy) / (xx * yy) : 0 };
}

function predictLock(data, settings, index, slip) {
  const p = data?.player,
    wheel = p?.wheels?.[index];
  const angle = slipAngle(wheel),
    goal = (settings.slipThreshold ?? 20) / 100;
  if (
    !data?.driving ||
    !(p?.brake > 0.02) ||
    !slip ||
    slip.speed < (settings.minimumSpeed ?? 20)
  )
    return null;
  if (slip.slip >= goal)
    return { locked: true, eta: 0, threshold: p.brake, angle };
  if (angle === null || slip.slip < 0.03) return null;
  const demand = combinedSlip(slip.slip, angle);
  if (demand >= goal)
    return { locked: false, eta: 0, threshold: p.brake, angle };
  // Extrapolate combined longitudinal and lateral demand at comparable speed/load.
  // Changing slip angle now advances or delays the warning instead of discarding it.
  const samples = (data.brakeTrace?.[index] || []).filter(
    (s) =>
      p.et - s.time >= 0 &&
      p.et - s.time <= 0.4 &&
      Math.abs(s.speed - slip.speed) <= Math.max(8, slip.speed * 0.15) &&
      s.load / wheel.load >= 0.65 &&
      s.load / wheel.load <= 1.35,
  );
  if (
    samples.length < 4 ||
    p.et - samples[0].time < 0.1 ||
    p.et - samples.at(-1).time > 0.1 ||
    slip.slip < 0.03
  )
    return null;
  const sliding = regression(
      samples.map((s) => ({ ...s, demand: combinedSlip(s.slip, s.angle) })),
      "demand",
    ),
    pedal = regression(samples, "brake");
  if (sliding.slope < 0.05 || sliding.fit < 0.75 || pedal.slope < -0.03)
    return null;
  const eta = (goal - demand) / sliding.slope;
  const threshold = p.brake + Math.max(0, pedal.slope) * eta;
  if (eta <= 0 || eta > (settings.predictionMs ?? 500) / 1000 || threshold > 1)
    return null;
  return { locked: false, eta, threshold, angle };
}
module.exports = { BrakeTrace, predictLock, slipAngle };
