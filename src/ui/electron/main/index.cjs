const {
  app,
  BrowserWindow,
  ipcMain,
  Tray,
  Menu,
  protocol,
  net,
  session,
} = require("electron");
const path = require("node:path");
const fs = require("node:fs");
const { pathToFileURL } = require("node:url");
const { execFile } = require("node:child_process");
const { promisify } = require("node:util");
const { Runtime, resolvePython } = require("./runtime.cjs");
const {
  defaults,
  keys,
  readSettings,
  writeSettings,
  validateSettings,
} = require("./settings.cjs");

const root = path.resolve(__dirname, "../../../..");
const preview = process.argv.includes("--preview");
const smoke = preview && process.argv.includes("--smoke");
const previewRoot = path.join(root, "work/electron-preview");
if (preview) app.setPath("userData", previewRoot);
app.setName("Sector");
app.setAppUserModelId("Sector.Desktop");
protocol.registerSchemesAsPrivileged([
  {
    scheme: "sector",
    privileges: { standard: true, secure: true, supportFetchAPI: true },
  },
]);
const settingsFile = path.join(
  preview ? previewRoot : process.env.LOCALAPPDATA || app.getPath("userData"),
  "LMUActiveReset/settings.json",
);
let preferences = { ...defaults };
let settingsError = "";
try {
  preferences = readSettings(settingsFile);
} catch (error) {
  settingsError = error.message;
}
const runtime = new Runtime({
  root,
  settingsFile,
  getSettings: () => preferences,
});
let window;
let tray;
let quitting = false;
let canQuit = false;
let audioReady;

function showWindow() {
  if (!window || window.isDestroyed()) return;
  if (window.isMinimized()) window.restore();
  window.show();
  window.focus();
}

async function quit() {
  if (quitting) return;
  quitting = true;
  showWindow();
  try {
    await runtime.stop();
    canQuit = true;
    app.quit();
  } catch (error) {
    quitting = false;
    runtime.publish({
      phase: "error",
      message: `Could not request shutdown: ${error.message}. Try Quit again.`,
    });
  }
}

function handle(channel, callback) {
  ipcMain.handle(channel, async (event, ...args) => {
    if (
      event.sender !== window?.webContents ||
      event.senderFrame !== window.webContents.mainFrame ||
      event.senderFrame.url !== "sector://app/index.html"
    )
      throw new Error("Untrusted desktop request.");
    return callback(...args);
  });
}

function installBridge() {
  handle("sector:initialize", () => ({
    settings: preferences,
    keys,
    status: runtime.state,
    preview,
    settingsError,
  }));
  handle("sector:settings", (candidate) => {
    try {
      preferences = preview
        ? validateSettings(candidate)
        : writeSettings(settingsFile, candidate);
      settingsError = "";
      if (!preview && !runtime.timer) runtime.start();
      return { ok: true, settings: preferences };
    } catch (error) {
      return { ok: false, error: error.message };
    }
  });
  handle("sector:sound", async (name) => {
    if (!["save", "restore"].includes(name) || !preferences.sound)
      return { ok: false, error: "Sound is disabled." };
    try {
      const directory = path.join(root, "work/ui-audio");
      if (!audioReady) {
        audioReady = promisify(execFile)(
          resolvePython(),
          [
            path.join(root, "scripts/sector.py"),
            "desktop.reset_feedback",
            directory,
          ],
          { cwd: root, windowsHide: true, timeout: 15000 },
        ).catch((error) => {
          audioReady = null;
          throw error;
        });
      }
      await audioReady;
      return {
        ok: true,
        source: `data:audio/wav;base64,${fs.readFileSync(path.join(directory, `${name}.wav`)).toString("base64")}`,
      };
    } catch {
      return {
        ok: false,
        error: "Sound preview unavailable. Check Python and your audio device.",
      };
    }
  });
  handle("sector:minimize", () => window.minimize());
  handle("sector:maximize", () =>
    window.isMaximized() ? window.unmaximize() : window.maximize(),
  );
  handle("sector:background", () => {
    if (!quitting) window.hide();
  });
  handle("sector:quit", () => {
    void quit();
  });
}

if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on("second-instance", showWindow);
  app.on("before-quit", (event) => {
    if (!canQuit) {
      event.preventDefault();
      void quit();
    }
  });
  app.on("window-all-closed", () => {
    /* Tray owns the app lifetime. */
  });
  app
    .whenReady()
    .then(async () => {
      const dist = path.join(__dirname, "../dist");
      protocol.handle("sector", (request) => {
        const url = new URL(request.url);
        const file = path.resolve(dist, `.${decodeURIComponent(url.pathname)}`);
        const relative = path.relative(dist, file);
        if (
          url.host !== "app" ||
          request.method !== "GET" ||
          relative.startsWith("..") ||
          path.isAbsolute(relative)
        )
          return new Response("Forbidden", { status: 403 });
        return net.fetch(pathToFileURL(file).toString());
      });
      session.defaultSession.setPermissionRequestHandler(
        (_contents, _permission, callback) => callback(false),
      );
      session.defaultSession.setPermissionCheckHandler(() => false);
      window = new BrowserWindow({
        title: "Sector",
        width: 1100,
        height: 810,
        minWidth: 620,
        minHeight: 560,
        backgroundColor: "#101319",
        frame: false,
        show: false,
        icon: path.join(root, "assets/sector.ico"),
        webPreferences: {
          preload: path.join(__dirname, "preload.cjs"),
          contextIsolation: true,
          nodeIntegration: false,
          sandbox: true,
          webSecurity: true,
          backgroundThrottling: true,
        },
      });
      window.removeMenu();
      window.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
      window.webContents.on("will-navigate", (event) => event.preventDefault());
      window.webContents.on("will-attach-webview", (event) =>
        event.preventDefault(),
      );
      window.webContents.on("render-process-gone", () => {
        void quit();
      });
      window.on("close", (event) => {
        if (!canQuit) {
          event.preventDefault();
          if (!quitting) window.webContents.send("sector:close-requested");
        }
      });
      installBridge();
      runtime.on("status", (state) => {
        if (!window.isDestroyed())
          window.webContents.send("sector:status", state);
      });
      tray = new Tray(path.join(root, "assets/sector.ico"));
      tray.setToolTip(preview ? "Sector — Preview" : "Sector");
      tray.setContextMenu(
        Menu.buildFromTemplate([
          { label: "Open Sector", click: showWindow },
          { type: "separator" },
          {
            label: "Quit Sector",
            click: () => {
              void quit();
            },
          },
        ]),
      );
      tray.on("double-click", showWindow);
      await window.loadURL("sector://app/index.html");
      if (!smoke) showWindow();
      // Preview and automated UI tests cannot discover LMU or launch the host.
      if (!preview && !settingsError) {
        try {
          preferences = writeSettings(settingsFile, preferences);
          runtime.start();
        } catch (error) {
          runtime.publish({ phase: "error", message: error.message });
        }
      }
      // Only the isolated preview process exposes a test driver, never the renderer.
      if (smoke)
        global.sectorTest = {
          publish: (state) => runtime.publish(state),
          show: showWindow,
          window: () => window,
          hasListener: () => Boolean(runtime.child || runtime.timer),
        };
    })
    .catch((error) => {
      console.error(error);
      canQuit = true;
      app.exit(1);
    });
}
