const { BrowserWindow, ipcMain, screen } = require("electron");
const path = require("node:path");
const model = require("../shared/overlay-model.cjs");
const store = require("../shared/overlay-config.cjs");
const { OverlayTelemetry } = require("./overlay-telemetry.cjs");
class Overlays {
  constructor({ root, file, preview, desktop, discover, ready = () => true }) {
    Object.assign(this, { root, file, preview, desktop, ready });
    this.windows = new Map();
    this.editing = false;
    this.error = "";
    try {
      this.config = store.read(file);
    } catch (e) {
      this.config = store.defaults();
      this.error = e.message;
    }
    this.telemetry = new OverlayTelemetry(root, { discover });
    this.telemetry.on("frame", () => this.sync());
    this.changed = () => this.sync();
    screen.on("display-added", this.changed);
    screen.on("display-removed", this.changed);
    screen.on("display-metrics-changed", this.changed);
    this.installBridge();
    this.sync();
  }
  state() {
    return {
      config: this.config,
      editing: this.editing,
      error: this.error,
      status: this.telemetry.status,
      frame: this.telemetry.frame,
      definitions: store.definitions,
      displays: screen.getAllDisplays().map((d) => ({
        id: d.id,
        primary: d.id === screen.getPrimaryDisplay().id,
        label: d.label || `${d.size.width} \u00d7 ${d.size.height}`,
        bounds: d.bounds,
      })),
    };
  }
  save(value) {
    try {
      this.config = this.preview
        ? store.validate(value)
        : store.write(this.file, value);
      this.error = "";
      this.sync();
      return { ok: true, ...this.state() };
    } catch (e) {
      this.error = e.message;
      return { ok: false, error: e.message };
    }
  }
  setEditing(value) {
    if (typeof value !== "boolean") throw new Error("Invalid edit mode.");
    if (!value) this.flushScale();
    this.editing = value;
    this.sync();
    return this.state();
  }
  send(win, id) {
    if (win.isDestroyed() || win.webContents.isLoading()) return;
    const { config, editing, frame } = this.state();
    win.webContents.send("sector:overlay-frame", {
      id,
      settings: config.widgets[id],
      editing,
      frame,
      scale: this.windowBounds(id).scale,
    });
  }
  windowBounds(id) {
    const display =
      screen.getAllDisplays().find((d) => d.id === this.config.displayId) ||
      screen.getPrimaryDisplay();
    return store.bounds(
      id,
      this.config.widgets[id],
      display.bounds,
      this.telemetry.frame
        ? model.valuesFor(id, this.telemetry.frame, this.config.widgets[id])
        : undefined,
    );
  }
  create(id) {
    const b = this.windowBounds(id);
    const win = new BrowserWindow({
      title: `Sector - ${store.definitions[id].name}`,
      x: b.x,
      y: b.y,
      width: b.width,
      height: b.height,
      transparent: true,
      frame: false,
      hasShadow: false,
      alwaysOnTop: true,
      skipTaskbar: true,
      resizable: false,
      show: false,
      focusable: false,
      webPreferences: {
        preload: path.join(__dirname, "overlay-preload.cjs"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: true,
        webSecurity: true,
        backgroundThrottling: false,
      },
    });
    this.windows.set(id, win);
    win.setAlwaysOnTop(true, "screen-saver");
    win.setIgnoreMouseEvents(true, { forward: true });
    win.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
    win.webContents.on("will-navigate", (e) => e.preventDefault());
    win.webContents.on("will-attach-webview", (e) => e.preventDefault());
    win.webContents.on("did-stop-loading", () => this.sync());
    win.webContents.on("render-process-gone", () => {
      this.error = "Overlay renderer closed. Toggle the widget to reopen it.";
      this.config.widgets[id].enabled = false;
      win.destroy();
      this.sync();
    });
    win.on("moved", () => {
      if (!this.editing || this.moving) return;
      const bounds = win.getBounds(),
        old = this.windowBounds(id);
      if (bounds.x === old.x && bounds.y === old.y) return;
      const display =
        screen.getAllDisplays().find((d) => d.id === this.config.displayId) ||
        screen.getPrimaryDisplay();
      this.config.widgets[id].x = Math.max(0, bounds.x - display.bounds.x);
      this.config.widgets[id].y = Math.max(0, bounds.y - display.bounds.y);
      clearTimeout(this.moveTimer);
      this.moveTimer = setTimeout(() => this.save(this.config), 200);
    });
    win.on("closed", () => this.windows.delete(id));
    void win.loadURL("sector://app/overlay.html");
    return win;
  }
  sync() {
    if (this.closed) return;
    const active = this.ready() && (this.config.enabled || this.editing);
    // Home uses session telemetry even when all overlay windows are disabled.
    if (this.ready() && !this.preview) this.telemetry.start();
    else if (!this.telemetry.stopped) this.telemetry.stop();
    for (const id of Object.keys(store.definitions)) {
      const setting = this.config.widgets[id];
      let win = this.windows.get(id);
      if (!active || !setting.enabled) {
        if (win) win.destroy();
        continue;
      }
      if (!win) win = this.create(id);
      const b = this.windowBounds(id),
        old = win.getBounds();
      if (["x", "y", "width", "height"].some((k) => b[k] !== old[k])) {
        this.moving = true;
        win.setBounds({ x: b.x, y: b.y, width: b.width, height: b.height });
        this.moving = false;
      }
      if (win.sectorEditing !== this.editing) {
        win.setFocusable(this.editing);
        win.setIgnoreMouseEvents(!this.editing, { forward: true });
        win.sectorEditing = this.editing;
      }
      const frame = this.telemetry.frame;
      if (
        !win.webContents.isLoading() &&
        (this.editing ||
          (this.config.enabled &&
            frame?.connected &&
            frame.cars?.length > 0 &&
            frame.foreground))
      ) {
        if (!win.isVisible()) win.showInactive();
      } else if (win.isVisible()) win.hide();
      this.send(win, id);
    }
    const desktop = this.desktop();
    if (desktop && !desktop.isDestroyed())
      desktop.webContents.send("sector:overlays", this.state());
  }
  installBridge() {
    const owner = (event) => {
      for (const [id, win] of this.windows)
        if (
          event.sender === win.webContents &&
          event.senderFrame === win.webContents.mainFrame &&
          event.senderFrame.url === "sector://app/overlay.html"
        )
          return id;
      throw new Error("Untrusted overlay request.");
    };
    ipcMain.handle("sector:overlay-ready", (event) => {
      const id = owner(event);
      return {
        id,
        settings: this.config.widgets[id],
        editing: this.editing,
        frame: this.telemetry.frame,
        scale: this.windowBounds(id).scale,
      };
    });
    ipcMain.handle("sector:overlay-scale", (event, scale, commit = true) => {
      const id = owner(event);
      if (
        !this.editing ||
        !Number.isFinite(scale) ||
        typeof commit !== "boolean"
      )
        return;
      this.config.widgets[id].scale = Math.max(0.5, Math.min(2, scale));
      // Resize native windows immediately; coalesce disk writes while dragging.
      clearTimeout(this.scaleTimer);
      if (commit) {
        this.scaleTimer = null;
        return this.save(this.config);
      }
      this.scaleTimer = setTimeout(() => this.flushScale(), 200);
      this.sync();
      return { ok: true };
    });
    ipcMain.handle("sector:overlay-lock", (event) => {
      owner(event);
      return this.setEditing(false);
    });
  }
  flushScale() {
    if (!this.scaleTimer) return;
    clearTimeout(this.scaleTimer);
    this.scaleTimer = null;
    this.save(this.config);
  }
  stop() {
    this.flushScale();
    this.closed = true;
    clearTimeout(this.moveTimer);
    this.telemetry.stop();
    for (const win of this.windows.values()) win.destroy();
    this.windows.clear();
    for (const event of [
      "display-added",
      "display-removed",
      "display-metrics-changed",
    ])
      screen.removeListener(event, this.changed);
  }
}
module.exports = { Overlays };
