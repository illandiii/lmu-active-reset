# Sector desktop

Sector 0.2.7 uses Electron, React and a locally built Vite renderer. The Python
supervisor and v20 native host own reset commands; the desktop displays their
acknowledged state. See [overlays](OVERLAYS.md) for the separate telemetry path.

## Launch and first-run setup

Open `Sector.exe` from the complete portable folder.

When switching versions, choose **Quit Sector** in the running app first. Closing
the window into background mode keeps that instance active. A second executable
brings the running instance forward and shows a notice explaining how to switch.

For a source checkout:

```powershell
./scripts/launch/Start-ResetGui.ps1
```

The source launcher needs Node.js 22.12+ and npm, installs locked dependencies
when necessary and builds the renderer. The backend also requires Python and
locally generated native binaries with matching compatibility profiles. See
[release prerequisites](RELEASE.md). Startup failures are logged to
`logs/desktop-startup.log` in the source workspace.

First launch searches Steam libraries for LMU. Confirm the proposed installation
or browse to a folder containing `Le Mans Ultimate.exe`. Both reset and telemetry
discovery use that choice. Setup offers a short tutorial and saves completion
independently of runtime versions. If the saved installation disappears, setup
is shown again.

For an isolated UI preview:

```powershell
./scripts/launch/Start-ResetGui.ps1 -Preview
```

Preview does not start the game detector, telemetry reader or reset host. It uses
an isolated profile under `work/electron-preview`, not the real preferences.

## Controls and menus

| Location | Controls |
| --- | --- |
| Home | Session summary, Save/Restore actions, overlay layout and Launch LMU |
| Active Reset | Checkpoint availability, save/restore shortcuts and confirmation sounds |
| Overlay | Widget visibility, layout editor and per-widget configuration |
| Edit → Settings | Reduced motion and overlay display selection |
| Help → Guide | Optional guided tour with anchored popups and arrows |
| Help → Changelog | Scrollable version history, newest first |
| Window close | Run in background, Quit Sector or Cancel |

Home is the initial tab. Launch LMU starts the validated installation's executable
with its own working directory and no shell. Process discovery prevents duplicate
launches and confirms the running state; preview mode cannot start the game.
The session card uses the same read-only telemetry reader as overlays, including
when overlay windows are disabled. Missing session values stay empty.

The connection/result status bar belongs to Active Reset. There is no connection
banner on the Overlay page. The optional tour never triggers a save or restore;
Next, Previous, Skip and Escape support keyboard navigation. Onboarding and guide
transitions respect both the app preference and the operating system's reduced
motion setting.

### Shortcuts and feedback

F6 saves; F7 restores. In Active Reset, click a shortcut and press the desired key. Escape or focus
loss cancels capture. Duplicates, held repeats and modifier combinations are
rejected. Tab navigates the dialog; its **Use Tab** button explicitly assigns Tab.
The shared key map covers letters, digits, F1–F24, navigation, numpad and OEM keys.

After changing a shortcut, release both keys before using them in LMU. Sector
observes keys without swallowing them, so avoid existing game bindings. Sound
previews use the listener's generated WAV files; actual success feedback depends
on native acknowledgement.

### Preferences

Normal preferences live in `%LOCALAPPDATA%/LMUActiveReset/`:

| File | Purpose |
| --- | --- |
| `settings.json` | Reset shortcuts, sound and app preferences |
| `overlays.json` | Widget visibility, geometry, sections and display |
| `onboarding.json` | Selected LMU installation and first-run completion |

Writes are validated and replaced atomically. Invalid settings block automatic
attachment and offer an explicit reset to defaults. Write failures are displayed.
The Python reader applies key changes when no command is in flight and requires
a fresh key release.

## Runtime ownership and shutdown

The main process discovers LMU, starts the Python supervisor through
`scripts/sector.py runtime.start_reset_hotkeys`, maintains a heartbeat and reads
versioned status snapshots. The supervisor waits for eligible driving, validates
compatibility and owns native lifecycle checks and key polling. Electron does
not register global reset shortcuts. Home exposes only explicit save/restore
requests through the trusted desktop IPC bridge; preview mode rejects them.

Each request has a unique ID, the active native host identity and a two-second
admission window. The supervisor consumes it once, rejects busy or ineligible
contexts and uses the same command IDs and prepare/restore gates as the hotkeys.
Desktop intent does not require game keyboard focus, but still requires admitted
offline driving. Only a matching native acknowledgement confirms success. The
desktop waits up to seven seconds and never automatically retries a request.
Session changes, shutdown and lost context cancel pending requests.

A single-instance lock prevents duplicate desktop listeners. Background mode
keeps the heartbeat active and locks the overlay layout. The tray or another
launch restores the same window. The window hides after the close-dialog exit
animation has completed.

The supplied Sector splash plays once per process launch, before Home or first-run
setup. Its SVG letter outlines draw in sequence before the filled word and blue
dot appear. Click or press a key to skip; the input is consumed so it cannot
activate the screen underneath. System and app reduced-motion settings use a
short fade instead. The main process owns the one-time launch flag, so tray
restores, second-instance launches and renderer reloads do not replay the splash.

Quit requests a cooperative stop and waits for cleanup. It does not terminate
LMU or interrupt an active restore by force. Heartbeat expiry handles unexpected
desktop termination. Stopped native DLLs remain resident until LMU exits.

A clean vehicle reconstruction clears the checkpoint and returns the supervisor
to waiting. Pause or focus loss alone does not discard it. Unexpected listener
failure is reported and blocks automatic retry against that same game instance.

## Renderer boundary and design

Electron's main process owns windows, tray, settings and subprocesses. The
sandboxed preload exposes named operations rather than generic IPC or filesystem
access. Context isolation and web security stay enabled; IPC validates the main
frame and local URL. The bounded `sector://app` protocol serves local renderer
assets. Navigation, new windows, webviews and permission requests are denied.

The interface uses local fonts and SVG icons, graphite surfaces and a blue accent.
Short transform/opacity transitions communicate state changes without delaying
key handling. Base UI provides dialog focus containment and keyboard tab behavior.
See [asset notices](../src/ui/electron/THIRD_PARTY_NOTICES.md).

Design references used during the Electron migration:

- [MengTo/Skills](https://github.com/MengTo/Skills), revision `83a47fee32f0b6349bff1fede99257a5ef03dc93`: `build-awwwards-quality-sites` and `dark-blue-contrasting-clean`.
- [emilkowalski/skills](https://github.com/emilkowalski/skills), revision `2d228630ba1446365f614a108cb8252f1feda202`: `emil-design-eng`, animation recipes and `pick-ui-library`.

These references guided layout and motion. Their sites are not runtime dependencies.

## Validation

Run `npm run format:check`, `npm test`, `npm run build` and `npm run test:ui`
from `src/ui/electron`; run `npm run test:onboarding` for first-run changes.
Desktop smoke tests exercise the actual Electron window in isolated preview.
See [test coverage](TEST_RESULTS.md) for what these checks establish and what
still requires a live LMU session.
