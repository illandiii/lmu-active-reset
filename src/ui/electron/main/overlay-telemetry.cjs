const { EventEmitter } = require("node:events");
const { spawn } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");
const { discoverGame } = require("./runtime.cjs");
const { TelemetryModel } = require("../shared/overlay-model.cjs");
class OverlayTelemetry extends EventEmitter {
  constructor(
    root,
    { discover = discoverGame, spawnChild = spawn, now = Date.now } = {},
  ) {
    super();
    Object.assign(this, { root, discover, spawnChild, now });
    this.model = new TelemetryModel();
    this.frame = null;
    this.status = "Waiting for LMU";
    this.stopped = true;
    this.last = 0;
  }
  publish(frame, status) {
    this.frame = frame;
    this.status = status;
    this.emit("frame", frame);
  }
  start() {
    if (!this.stopped) return;
    this.stopped = false;
    this.timer = setInterval(() => void this.tick(), 500);
    void this.tick();
  }
  async tick() {
    if (this.stopped || this.busy) return;
    if (this.child) {
      if (this.now() - this.last > 1500 && this.frame)
        this.publish(null, "Waiting for telemetry");
      return;
    }
    if (this.now() < (this.retry || 0)) return;
    this.busy = true;
    this.retry = this.now() + 2500;
    try {
      const binary = path.join(this.root, "bin/overlay-reader.exe");
      if (!fs.existsSync(binary)) {
        this.publish(null, "Build the overlay reader");
        return;
      }
      const games = await this.discover();
      if (this.stopped) return;
      if (games.length !== 1) {
        this.publish(
          null,
          games.length ? "Select one LMU instance" : "Waiting for LMU",
        );
        return;
      }
      const child = this.spawnChild(
        binary,
        [String(games[0].pid), String(process.pid)],
        { windowsHide: true, stdio: ["pipe", "pipe", "ignore"] },
      );
      this.child = child;
      this.last = this.now();
      this.model.reset();
      let buffer = "";
      child.stdin.on("error", () => {});
      child.stdout.setEncoding("utf8");
      child.stdout.on("data", (chunk) => {
        buffer += chunk;
        if (buffer.length > 1024 * 1024) {
          buffer = "";
          return;
        }
        let end;
        while ((end = buffer.indexOf("\n")) >= 0) {
          const line = buffer.slice(0, end);
          buffer = buffer.slice(end + 1);
          try {
            const raw = JSON.parse(line);
            if (
              raw.schema !== 1 ||
              !Array.isArray(raw.cars) ||
              raw.cars.length > 104
            )
              continue;
            // SDK timestamps must advance. A paused mapping must not leave a frozen HUD on screen.
            const now = this.now();
            if (raw.player?.et !== this.lastET) {
              this.lastTelemetry = now;
              this.lastET = raw.player?.et;
            }
            if (raw.et !== this.lastETScoring) {
              this.lastScoring = now;
              this.lastETScoring = raw.et;
            }
            this.last = raw.player
              ? Math.min(this.lastTelemetry ?? now, this.lastScoring ?? now)
              : (this.lastScoring ?? now);
            if (now - this.last > 1500) continue;
            this.publish(
              this.model.update(raw),
              raw.driving ? "Live" : "Waiting for driving",
            );
          } catch {
            this.publish(null, "Telemetry unavailable");
          }
        }
      });
      const ended = () => {
        if (this.child === child) {
          this.child = null;
          this.lastET = null;
          this.lastETScoring = null;
          this.publish(null, "Waiting for telemetry");
        }
      };
      child.on("error", ended);
      child.on("exit", ended);
    } catch {
      this.publish(null, "Telemetry unavailable");
    } finally {
      this.busy = false;
    }
  }
  stop() {
    this.stopped = true;
    clearInterval(this.timer);
    this.timer = null;
    this.child?.stdin.end("stop\n");
    this.publish(null, "Disabled");
  }
}
module.exports = { OverlayTelemetry };
