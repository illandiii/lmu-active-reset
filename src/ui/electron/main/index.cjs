const {
  app,
  BrowserWindow,
  ipcMain,
  Tray,
  Menu,
  protocol,
  net,
  session,
  dialog,
} = require("electron");
const path = require("node:path");
const fs = require("node:fs");
const { pathToFileURL } = require("node:url");
const { execFile } = require("node:child_process");
const { promisify } = require("node:util");
const { Overlays } = require("./overlays.cjs");
const { Runtime, resolvePython, discoverGame } = require("./runtime.cjs");
const setupStore = require("./onboarding.cjs");
const { GameLauncher } = require("./game-launcher.cjs");
const {
  defaults,
  keys,
  readSettings,
  writeSettings,
  validateSettings,
} = require("./settings.cjs");

// Packaged resources are copied to a versioned writable workspace for native logs and IPC.
const sourceRoot = path.resolve(__dirname, "../../../..");
const packagedPreview = process.argv.includes("--preview");
const root = app.isPackaged
  ? path.join(
      app.getPath("userData"),
      packagedPreview ? "preview-runtime" : "runtime",
      `${app.getVersion()}-${fs.readFileSync(path.join(process.resourcesPath, "release-id"), "utf8").trim()}`,
    )
  : sourceRoot;
if (app.isPackaged) {
  if (!fs.existsSync(root)) {
    const staging = `${root}-${process.pid}.tmp`;
    fs.mkdirSync(staging, { recursive: true });
    fs.cpSync(path.join(process.resourcesPath, "runtime"), staging, {
      recursive: true,
    });
    try {
      fs.renameSync(staging, root);
    } catch (error) {
      if (!fs.existsSync(root)) throw error;
    }
  }
  process.env.SECTOR_PYTHON = path.join(root, "python/python.exe");
}
const preview = process.argv.includes("--preview");
const smoke = preview && process.argv.includes("--smoke");
const onboardingSmoke = smoke && process.argv.includes("--onboarding-smoke");
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
const setupFile =
  onboardingSmoke && process.env.SECTOR_TEST_SETUP_FILE
    ? process.env.SECTOR_TEST_SETUP_FILE
    : path.join(path.dirname(settingsFile), "onboarding.json");
let setup = setupStore.readSetup(setupFile);
const setupReady = () => (preview && !onboardingSmoke) || setup.completed;
const gameLauncher = new GameLauncher({
  installation: () => setup.lmuPath,
  discover: discoverGame,
  preview,
});
const discoverInstallation = async () => {
  if (!setupReady()) return [];
  const games = await discoverGame(setup.lmuPath);
  gameLauncher.observe(games);
  return games;
};
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
  discover: discoverInstallation,
});
let window;
let tray;
let quitting = false;
let canQuit = false;
let audioReady;
let overlays;
// Keep this in the main process so hiding, restoring or reloading cannot replay it.
let launchAnimationPending = true;
gameLauncher.on("state", (state) => {
  if (window && !window.isDestroyed())
    window.webContents.send("sector:game", state);
});

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
    overlays?.stop();
    gameLauncher.stop();
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
  handle("sector:initialize", () => {
    const launchAnimation = launchAnimationPending;
    launchAnimationPending = false;
    return {
      launchAnimation,
      settings: preferences,
      keys,
      status: runtime.state,
      preview,
      game: gameLauncher.state,
      settingsError,
      onboarding: { required: !setupReady(), folder: setup.lmuPath },
    };
  });
  handle("sector:launch-game", () => gameLauncher.launch());
  handle("sector:reset", (action) =>
    preview || quitting || !setupReady()
      ? { ok: false, error: "Reset is unavailable in this session." }
      : runtime.requestReset(action),
  );
  handle("sector:setup-detect", async () => {
    if (preview)
      return onboardingSmoke && process.env.SECTOR_TEST_LMU
        ? [setupStore.validateFolder(process.env.SECTOR_TEST_LMU)]
        : [];
    return setupStore.detectInstallations();
  });
  handle("sector:setup-browse", async () => {
    const result = await dialog.showOpenDialog(window, {
      title: "Select Le Mans Ultimate folder",
      defaultPath: setup.lmuPath || undefined,
      properties: ["openDirectory"],
    });
    if (result.canceled || !result.filePaths[0]) return { canceled: true };
    try {
      return {
        ok: true,
        folder: setupStore.validateFolder(result.filePaths[0]),
      };
    } catch (error) {
      return { ok: false, error: error.message };
    }
  });
  handle("sector:setup-confirm", (folder) => {
    try {
      if (setupReady()) throw new Error("Setup is already complete.");
      setup = setupStore.writeSetup(setupFile, {
        lmuPath: folder,
        completed: false,
      });
      return { ok: true, folder: setup.lmuPath };
    } catch (error) {
      return { ok: false, error: error.message };
    }
  });
  handle("sector:setup-complete", () => {
    try {
      setup = setupStore.writeSetup(setupFile, { ...setup, completed: true });
      if (!preview && !settingsError) {
        preferences = writeSettings(settingsFile, preferences);
        runtime.start();
      }
      overlays.sync();
      return { ok: true };
    } catch (error) {
      return { ok: false, error: error.message };
    }
  });
  handle("sector:settings", (candidate) => {
    try {
      preferences = preview
        ? validateSettings(candidate)
        : writeSettings(settingsFile, candidate);
      settingsError = "";
      if (!preview && setupReady() && !runtime.timer) runtime.start();
      return { ok: true, settings: preferences };
    } catch (error) {
      return { ok: false, error: error.message };
    }
  });
  handle("sector:overlays-initialize", () => overlays.state());
  handle("sector:overlays-save", (value) => overlays.save(value));
  handle("sector:overlays-edit", (value) => overlays.setEditing(value));
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
    if (!quitting) {
      overlays.setEditing(false);
      window.hide();
    }
  });
  handle("sector:quit", () => {
    void quit();
  });
}

if (!app.requestSingleInstanceLock()) {
  // An older tray instance otherwise silently receives launches of a new EXE.
  if (preview) app.quit();
  else
    app.whenReady().then(async () => {
      await dialog.showMessageBox({
        type: "info",
        title: `Sector ${app.getVersion()}`,
        message: "Sector is already running",
        detail: `The open instance has been brought to the front. To start version ${app.getVersion()}, choose Quit Sector in that app, then open this executable again.`,
        buttons: ["OK"],
      });
      app.quit();
    });
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
        width: 1000,
        height: 720,
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
      overlays = new Overlays({
        root,
        file: path.join(path.dirname(settingsFile), "overlays.json"),
        preview,
        desktop: () => window,
        discover: discoverInstallation,
        ready: setupReady,
      });
      installBridge();
      runtime.on("status", (state) => {
        overlays.telemetry.onResetStatus(state);
        if (!window.isDestroyed())
          window.webContents.send("sector:status", state);
      });
      tray = new Tray(path.join(root, "assets/sector.ico"));
      tray.setToolTip(preview ? "Sector - Preview" : "Sector");
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
      if (!preview && setupReady() && !settingsError) {
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
          overlays: () => overlays,
        };
    })
    .catch((error) => {
      console.error(error);
      canQuit = true;
      app.exit(1);
    });
}
