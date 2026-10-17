// A localized sliding-distance estimate, not an SDK damage measurement.
// Thresholds are intentionally conservative and need live-game calibration.
const emptyPatch = () => ({ exposure: 0, duration: 0, angle: 0 });
const emptyWheel = () => ({ patch: emptyPatch(), peak: 0, duration: 0 });

class FlatSpotHistory {
  constructor() {
    this.wheels = Array.from({ length: 4 }, emptyWheel);
    this.previous = null;
  }

  update(time, contacts, replaced) {
    const previous = this.previous;
    const dt = time - previous?.time;
    if (Number.isFinite(dt) && dt < 0)
      this.wheels = Array.from({ length: 4 }, emptyWheel);
    for (let i = 0; i < 4; i++) {
      if (replaced[i]) this.wheels[i] = emptyWheel();
      const state = this.wheels[i];
      const current = contacts[i],
        old = previous?.contacts[i];
      // Duplicate samples neither add exposure nor interrupt a genuine lock.
      if (dt === 0 && current && old && !replaced[i]) continue;
      if (!current || !old || !(dt > 0 && dt <= 0.15) || replaced[i]) {
        state.patch = emptyPatch();
        continue;
      }
      const angle = (current.rotation + old.rotation) * 0.5 * dt;
      // Once the wheel rotates past this patch, do not accumulate whole-tyre wear.
      if (state.patch.angle + angle > Math.PI / 6) state.patch = emptyPatch();
      if (angle > Math.PI / 6) continue;
      state.patch.angle += angle;
      state.patch.duration += dt;
      state.patch.exposure +=
        Math.min(current.sliding, old.sliding) *
        Math.min(current.loadFactor, old.loadFactor) *
        dt;
      if (state.patch.duration >= 0.2 && state.patch.exposure > state.peak) {
        state.peak = state.patch.exposure;
        state.duration = state.patch.duration;
      }
    }
    this.previous = { time, contacts };
    return this.wheels.map(({ peak, duration }) => ({
      exposure: peak,
      duration,
    }));
  }
}

function flatSpotDetected(estimate, sensitivity = 50) {
  // Equivalent sliding metres at 3 kN. Higher sensitivity lowers the threshold.
  const threshold = 40 * (50 / Math.max(10, Math.min(100, sensitivity)));
  return (
    Number.isFinite(estimate?.exposure) &&
    Number.isFinite(estimate?.duration) &&
    estimate.duration >= 0.2 &&
    estimate.exposure >= threshold
  );
}

module.exports = { FlatSpotHistory, flatSpotDetected };
