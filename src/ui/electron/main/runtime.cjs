const fs = require("node:fs");
const path = require("node:path");
const { EventEmitter } = require("node:events");
const { randomUUID } = require("node:crypto");
const { spawn, execFile } = require("node:child_process");
const { promisify } = require("node:util");
const execute = promisify(execFile);

const emptyCheckpoint = {
  checkpoint: false,
  saved_speed_kmh: null,
  saved_at: null,
  restore_count: 0,
};

async function discoverGame(installation) {
  // This fixed command never includes data from the renderer or settings.
  const command =
    "$env:PSModulePath = (Join-Path $PSHOME 'Modules') + [IO.Path]::PathSeparator + $env:PSModulePath; @(Get-Process -Name 'Le Mans Ultimate' -ErrorAction SilentlyContinue | ForEach-Object { @{pid=$_.Id; executable=$_.Path; identity=([string]$_.Id + ':' + $_.StartTime.ToUniversalTime().Ticks)} }) | ConvertTo-Json -Compress";
  const { stdout } = await execute(
    "powershell.exe",
    ["-NoProfile", "-NonInteractive", "-Command", command],
    { windowsHide: true, timeout: 5000 },
  );
  const parsed = stdout.trim() ? JSON.parse(stdout) : [];
  const games = Array.isArray(parsed) ? parsed : [parsed];
  return installation
    ? games.filter(
        (game) =>
          typeof game.executable === "string" &&
          path.resolve(game.executable).toLowerCase() ===
            path.join(installation, "Le Mans Ultimate.exe").toLowerCase(),
      )
    : games;
}

function resolvePython() {
  if (process.env.SECTOR_PYTHON) return process.env.SECTOR_PYTHON;
  const bundled = path.join(
    process.env.USERPROFILE || "",
    ".cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe",
  );
  return fs.existsSync(bundled) ? bundled : "python.exe";
}

class Runtime extends EventEmitter {
  constructor({
    root,
    settingsFile,
    getSettings,
    python = resolvePython(),
    discover = discoverGame,
    spawnChild = spawn,
    now = Date.now,
  }) {
    super();
    Object.assign(this, {
      root,
      settingsFile,
      getSettings,
      python,
      discover,
      spawnChild,
      now,
    });
    this.state = {
      ...emptyCheckpoint,
      phase: "idle",
      message: "",
    };
    this.child = null;
    this.blockedIdentity = null;
    this.nextDiscovery = 0;
    this.nextHeartbeat = 0;
    this.sequence = -1;
    this.stopping = false;
    this.busy = false;
  }

  publish(patch) {
    this.state = { ...this.state, ...patch };
    if (!this.state.checkpoint) Object.assign(this.state, emptyCheckpoint);
    this.emit("status", this.state);
  }

  start() {
    if (this.timer) return;
    this.timer = setInterval(() => this.tick(), 250);
    void this.tick();
  }

  requestReset(action) {
    const reject = (error) => Promise.resolve({ ok: false, error });
    if (!["save", "restore"].includes(action))
      return reject("Unknown reset action.");
    if (
      this.stopping ||
      !this.child ||
      this.state.phase !== "active" ||
      !this.state.log_prefix
    )
      return reject("Enter driving before saving or restoring.");
    if (this.resetRequest)
      return reject("A reset operation is already in progress.");
    if (action === "restore" && !this.state.checkpoint)
      return reject("Save a checkpoint before restoring.");
    const request = {
      schema_version: 1,
      id: randomUUID(),
      action,
      host: this.state.log_prefix,
      created_at_ms: this.now(),
    };
    return new Promise((resolve) => {
      const file = path.join(this.session, "command.json");
      const temporary = `${file}.tmp`;
      this.resetRequest = {
        id: request.id,
        resolve,
        file,
        timer: setTimeout(
          () =>
            this.finishReset({
              ok: false,
              error: "Reset acknowledgement timed out.",
            }),
          7000,
        ),
      };
      try {
        fs.writeFileSync(temporary, JSON.stringify(request));
        fs.renameSync(temporary, file);
      } catch (error) {
        this.finishReset({ ok: false, error: error.message });
      } finally {
        fs.rmSync(temporary, { force: true });
      }
    });
  }

  finishReset(result) {
    const pending = this.resetRequest;
    if (!pending) return;
    this.resetRequest = null;
    clearTimeout(pending.timer);
    // Delete only our own unconsumed request. Never retry a timed-out reset.
    try {
      if (JSON.parse(fs.readFileSync(pending.file, "utf8")).id === pending.id)
        fs.rmSync(pending.file);
    } catch {
      /* The supervisor may already have consumed it. */
    }
    pending.resolve(result);
  }

  async tick() {
    if (this.busy) return;
    this.busy = true;
    try {
      if (this.child) {
        if (this.now() >= this.nextHeartbeat) {
          fs.writeFileSync(path.join(this.session, "heartbeat"), "");
          this.nextHeartbeat = this.now() + 1000;
        }
        this.readStatus();
        return;
      }
      if (this.stopping || this.now() < this.nextDiscovery) return;
      this.nextDiscovery = this.now() + 1500;
      const games = await this.discover();
      // A quit request may arrive while PowerShell is discovering the game.
      if (this.stopping) return;
      if (games.length !== 1) {
        this.blockedIdentity = null;
        this.publish({
          phase: "idle",
          ...emptyCheckpoint,
          message: games.length > 1 ? "Keep one LMU instance open." : "",
        });
        return;
      }
      const game = games[0];
      if (
        !Number.isSafeInteger(game.pid) ||
        game.pid <= 0 ||
        typeof game.identity !== "string"
      )
        throw new Error("Invalid LMU process identity.");
      if (game.identity === this.blockedIdentity) return;
      this.blockedIdentity = game.identity;
      this.launch(game.pid);
    } catch (error) {
      this.publish({ phase: "error", message: error.message });
    } finally {
      this.busy = false;
    }
  }

  launch(pid) {
    this.session = path.join(this.root, "work/gui", randomUUID());
    fs.mkdirSync(this.session, { recursive: true });
    fs.writeFileSync(path.join(this.session, "heartbeat"), "");
    const logs = path.join(this.root, "logs");
    fs.mkdirSync(logs, { recursive: true });
    const name = `gui-${path.basename(this.session)}`;
    const preferences = this.getSettings();
    const args = [
      path.join(this.root, "scripts/sector.py"),
      "runtime.start_reset_hotkeys",
      "--pid",
      String(pid),
      "--save-key",
      preferences.save_key,
      "--restore-key",
      preferences.restore_key,
      "--gui-session",
      this.session,
      "--settings-file",
      this.settingsFile,
      "--offline-launch-confirmed",
      "--allow-writes",
    ];
    if (!preferences.sound) args.push("--mute");
    const output = fs.openSync(path.join(logs, `${name}.log`), "a");
    let errors;
    try {
      errors = fs.openSync(path.join(logs, `${name}.error.log`), "a");
      this.child = this.spawnChild(this.python, args, {
        cwd: this.root,
        windowsHide: true,
        stdio: ["ignore", output, errors],
        env: { ...process.env, PYTHONUTF8: "1" },
      });
    } finally {
      fs.closeSync(output);
      if (errors !== undefined) fs.closeSync(errors);
    }
    this.sequence = -1;
    this.publish({
      phase: "waiting",
      ...emptyCheckpoint,
      message: "LMU detected. Enter driving in offline Practice.",
    });
    const child = this.child;
    child.once("error", (error) => {
      this.publish({
        phase: "error",
        ...emptyCheckpoint,
        message: `Cannot start the reset listener: ${error.message}`,
      });
    });
    child.once("close", (code) => {
      if (this.child !== child) return;
      this.readStatus();
      this.finishReset({ ok: false, error: "Reset listener disconnected." });
      this.child = null;
      const previousError = this.state.phase === "error";
      this.publish({
        ...emptyCheckpoint,
        phase: previousError ? "error" : code === 0 ? "stopped" : "error",
        message: previousError
          ? this.state.message
          : code === 0
            ? "Disconnected."
            : `Listener stopped (${code ?? "launch error"}). Reopen Sector after checking the session log.`,
      });
      this.emit("detached");
    });
  }

  readStatus() {
    try {
      const value = JSON.parse(
        fs.readFileSync(path.join(this.session, "status.json"), "utf8"),
      );
      if (
        value.schema_version !== 1 ||
        !Number.isSafeInteger(value.sequence) ||
        value.sequence === this.sequence
      )
        return;
      this.sequence = value.sequence;
      this.publish(value);
      if (
        value.desktop_request?.id === this.resetRequest?.id &&
        this.resetRequest
      )
        this.finishReset(value.desktop_request);
    } catch {
      /* Atomic replacement can briefly race a reader on Windows. */
    }
  }

  async stop() {
    this.stopping = true;
    this.finishReset({ ok: false, error: "Sector is closing." });
    if (!this.child) {
      this.dispose();
      return;
    }
    this.publish({
      phase: "stopping",
      message: "Finishing the current operation…",
    });
    // Keep heartbeat/status polling alive until Python confirms native detach.
    // Never terminate the game or a listener in the middle of a restore.
    const detached = new Promise((resolve) => this.once("detached", resolve));
    fs.writeFileSync(path.join(this.session, "stop"), "");
    await detached;
    this.dispose();
  }

  dispose() {
    this.finishReset({ ok: false, error: "Reset listener stopped." });
    clearInterval(this.timer);
    this.timer = null;
  }
}

module.exports = { Runtime, resolvePython, emptyCheckpoint, discoverGame };
