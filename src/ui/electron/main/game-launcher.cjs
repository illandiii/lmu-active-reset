const { EventEmitter } = require("node:events");
const { spawn } = require("node:child_process");
const path = require("node:path");
const { validateFolder } = require("./onboarding.cjs");

class GameLauncher extends EventEmitter {
  constructor({ installation, discover, preview = false, spawnChild = spawn }) {
    super();
    Object.assign(this, { installation, discover, preview, spawnChild });
    this.state = { running: false, launching: false, error: "" };
  }

  publish(patch) {
    this.state = { ...this.state, ...patch };
    this.emit("state", this.state);
  }

  observe(games) {
    const running = games.length > 0;
    if (running) {
      clearTimeout(this.timeout);
      this.publish({ running: true, launching: false, error: "" });
    } else if (this.state.running) {
      this.publish({ running: false });
    }
  }

  async launch() {
    if (this.stopped) return { ok: false, error: "Sector is closing." };
    if (this.preview)
      return { ok: false, error: "Game launch is unavailable in preview." };
    if (this.state.launching || this.state.running) return { ok: true };
    this.publish({ launching: true, error: "" });
    try {
      const folder = validateFolder(this.installation());
      // Check all LMU processes, including copies outside the selected folder.
      const games = await this.discover();
      if (this.stopped) return { ok: false, error: "Sector is closing." };
      if (games.length) {
        this.observe(games);
        return { ok: true };
      }
      const child = this.spawnChild(
        path.join(folder, "Le Mans Ultimate.exe"),
        [],
        {
          cwd: folder,
          detached: true,
          stdio: "ignore",
          shell: false,
        },
      );
      await new Promise((resolve, reject) => {
        child.once("error", reject);
        child.once("spawn", resolve);
      });
      child.unref();
      // Process discovery, rather than a successful spawn, confirms the game opened.
      if (!this.stopped && !this.state.running)
        this.timeout = setTimeout(() => {
          this.publish({
            launching: false,
            error: "LMU has not opened. Check Steam, then try again.",
          });
        }, 45000);
      this.timeout?.unref?.();
      return { ok: true };
    } catch (error) {
      this.publish({
        launching: false,
        error: `Could not launch LMU: ${error.message}`,
      });
      return { ok: false, error: this.state.error };
    }
  }

  stop() {
    this.stopped = true;
    clearTimeout(this.timeout);
  }
}

module.exports = { GameLauncher };
