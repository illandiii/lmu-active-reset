// Model names are stable vehicle metadata, unlike short-lived opponent telemetry.
class VehicleIdentity {
  constructor() {
    this.reset();
  }
  reset() {
    this.entries = new Map();
    this.session = null;
    this.time = null;
  }
  update(raw) {
    if (!raw?.connected) {
      this.reset();
      return raw;
    }
    const session = JSON.stringify([raw.track, raw.session]);
    if (
      session !== this.session ||
      (Number.isFinite(raw.et) && raw.et < this.time)
    )
      this.reset();
    this.session = session;
    this.time = Number.isFinite(raw.et) ? raw.et : this.time;
    const present = new Set();
    const cars = (raw.cars || []).map((car) => {
      present.add(car.id);
      const identity = JSON.stringify([
        car.vehicleFile || "",
        car.vehicle || "",
        car.class || "",
      ]);
      const previous = this.entries.get(car.id);
      const measured =
        car.model || (raw.player?.id === car.id ? raw.player.model : "");
      const model =
        measured || (previous?.identity === identity ? previous.model : "");
      this.entries.set(car.id, { identity, model });
      return { ...car, model };
    });
    for (const id of this.entries.keys())
      if (!present.has(id)) this.entries.delete(id);
    return { ...raw, cars };
  }
}
module.exports = { VehicleIdentity };
