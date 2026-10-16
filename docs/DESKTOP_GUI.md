# Sector desktop — Electron

Sector 0.2 replaces the WPF window with Electron, React and a locally built Vite
renderer. `Start-Sector.cmd` and its `Start-Reset.cmd` alias open the new app.
The v15 physical reset host and Python hotkey supervisor remain the source of
truth. This UI migration does not alter native physics or reset admission.

## Setup

On the configured Windows x64 machine, install Node.js 22.12+ with npm. The
launcher installs the lockfile dependencies when needed, fetches Electron on
first use, builds the renderer, then opens Sector. First setup needs internet;
fonts, icons, scripts and sound previews are local after setup. Startup errors
point to `logs/desktop-startup.log`.

For an explicit UI build or an isolated preview:

```powershell
./scripts/build/Build-SectorUi.ps1
./scripts/launch/Start-ResetGui.ps1 -Preview
./scripts/test/Test-ResetGui.ps1 -IncludeElectron
```

Preview never starts the game detector or Python reset host,
and does not write real preferences. Its Electron profile is under
`work/electron-preview`. Tests use this same isolated mode.

The physical backend still needs Python 3.11+, local generated binaries and the
pinned compatibility manifest. `SECTOR_PYTHON` (or launcher `-PythonPath`) selects
Python; otherwise the configured local runtime, then `python.exe`, is used.
A public clone does not contain private installation manifests or recorded frames.
To rebuild the native backend on the configured machine, close LMU and Sector and
run `scripts/build/Build-ResetDesktop.ps1 -ZigPath C:\tools\zig\zig.exe`.

## Window and settings

- Active Reset shows acknowledged checkpoint availability, shortcut assignment
  and confirmation audio. The compact layout omits counters, saved time and speed.
- The header gear opens an embedded Settings dialog with Reduce motion.
  This preference is stored with existing settings;
  old files default to normal motion, while the OS reduced-motion setting is
  always respected.
- Connection phase and the latest command result live in the bottom status bar
  on both tabs. Page eyebrows, the old product footer and the blue card's
  Offline Practice label have been removed; the title bar only says sector.
- The Guide button beside each page title starts an optional seven-step tour.
  Anchored popups and arrows explain checkpoints, keys, audio, connection, settings,
  overlays and background mode. Next, Previous, Skip and Escape work with
  keyboard focus containment. Finishing returns to the starting tab; the tour
  never triggers a save or restore.
- Overlay contains seven configurable transparent widgets; see [OVERLAYS.md](OVERLAYS.md).
- Click a shortcut to capture one key. Escape or window focus loss cancels.
  Duplicates, held repeats and modifier combinations are rejected. Tab navigates
  the dialog; **Use Tab** explicitly assigns that key. Enter/Space activate a
  focused dialog button; from the capture surface they can be assigned.
- Save/restore defaults remain F6/F7. Letters, digits, F1–F24, navigation, numpad
  and OEM punctuation use the shared Python keymap. Windows virtual-key values
  preserve keyboard-layout mapping. Select keys not already bound in LMU.
- Sound previews use the same generated WAVs as the Python listener. Real success
  sounds and counters depend on native acknowledgements, never the keypress.
- Closing offers **Run in background**, **Quit Sector**, and **Cancel** inside
  the app window. The tray and a second launcher can reopen the existing window.

Settings remain at `%LOCALAPPDATA%/LMUActiveReset/settings.json`. Updates are
validated and atomically replaced; the existing Python settings reader applies
changes when no command is in flight and requires fresh key release. Invalid
stored preferences stop automatic attachment and offer explicit restoration of
defaults. Failed writes remain visible and do not pretend to apply preferences.

## Process ownership and shutdown

`main/runtime.cjs` discovers one LMU process, creates a unique `work/gui/<id>`
channel, writes its heartbeat, starts `scripts/sector.py runtime.start_reset_hotkeys`
with an argument array and reads versioned atomic `status.json` snapshots.
The existing supervisor waits for offline Practice driving, checks EAC and game
compatibility, and owns all native lifecycle checks and key polling. Electron
registers no global reset shortcuts and exposes no save/restore IPC command.

The main process owns the heartbeat even when the renderer is hidden. A
single-instance lock prevents duplicate desktop listeners; process identity
includes creation time to distinguish reused PIDs. Unexpected listener failure
is shown and blocks automatic retry against the same game instance. A clean
vehicle reconstruction is handled inside the Python supervisor, which clears the
old checkpoint and waits for driving before connecting a new host.

Quit creates the cooperative `stop` file and keeps status/heartbeat polling alive
until Python exits. It never kills LMU or forcibly terminates an active restore.
A quit arriving during process discovery cannot start a listener afterward. If
Electron crashes, heartbeat expiry requests native shutdown through the existing
watchdog. Stopped DLLs remain resident until LMU exits, as before.

A rejected replacement save retains the old checkpoint. Pause/focus loss alone
does not delete it; native identity or lifecycle invalidation does. No desktop
session timer, checkpoint age, speed or distance limit is introduced here.

## Renderer boundary

`main/index.cjs` owns window/tray, validated settings, sound previews and runtime.
The sandboxed `preload.cjs` exposes named operations, not Electron, Node, shell,
filesystem paths or generic IPC. Context isolation and web security stay enabled.
All IPC checks the exact main frame and local URL. Navigation, new windows,
webviews and permission requests are denied. A bounded `sector://app` protocol
serves only the built renderer; CSP blocks remote scripts and connections.
A future overlay may consume acknowledged status without becoming a second
controller or refreshing the desktop heartbeat.

## Design and motion sources

The user requested [MengTo/Skills](https://github.com/MengTo/Skills) and
[emilkowalski/skills](https://github.com/emilkowalski/skills). Applied references:

- MengTo `build-awwwards-quality-sites` and `dark-blue-contrasting-clean`,
  revision `83a47fee32f0b6349bff1fede99257a5ef03dc93`.
- Emil `emil-design-eng`, `animate`, its recipes and `pick-ui-library`,
  revision `2d228630ba1446365f614a108cb8252f1feda202`.

Direction: a composed trackside utility with graphite/navy surfaces, one cobalt
checkpoint focal region, Manrope interface type and Barlow Condensed numerals.
The established Sector mark remains; Solar Linear SVG icons are bundled locally.
The layout flows from feature tabs to checkpoint state, then shortcut/audio
settings. No reference identity, imagery or copy is reproduced.

The website-oriented skill is adapted to a desktop utility: native scrolling,
no scroll takeover, GSAP intro, Three.js scene or continuous decorative animation.
Emil's task-specific animation guidance takes precedence for repeated app use:
CSS transform/opacity transitions only, 120–200 ms, with
`cubic-bezier(0.23, 1, 0.32, 1)`. Tabs and switches communicate spatial changes;
modals use a centered 0.96-scale entry. Keyboard interactions and checkpoint
numbers update immediately. Reduced motion removes movement. Base UI owns dialog
focus containment and tab keyboard navigation. Controls have visible focus;
custom scrolling, compact layouts and forced-color fallbacks are included.

Asset attribution and font licenses live in
`src/ui/electron/THIRD_PARTY_NOTICES.md`. All UI source comments are English.

## Verification and limits

Run from `src/ui/electron`:

```powershell
npm run format:check
npm test
npm run build
npm run test:ui
```

The controller tests cover settings migration/atomic writes, invalid input,
keyboard mapping, acknowledged status, listener identity, discovery races and
cooperative stop. The real Electron smoke suite covers isolation, tabs, actual
key events, duplicate/modifier rejection, Escape, sound generation, mute,
acknowledged checkpoint updates, focus containment, background/reopen,
second-instance exclusion, compact layout, guided tour navigation and dismissal,
  reduced motion and quit. Screenshots
are written to ignored `work/electron-screenshots` and visually inspected.
Python regressions run with `python scripts/sector.py test`.

Verified on Windows on 16 October 2026: 9 desktop controller tests, 95 Python
tests, production build, formatting, and the Electron smoke suite all passed.
The smoke suite also runs the real PowerShell launcher to reveal the hidden
instance. Both launcher and game discovery prefer the current PowerShell
interpreter's modules, avoiding a PowerShell 7 module path inherited by Windows
PowerShell through Node.

These are desktop/fixture checks, not a new live-game reset qualification. The
seven successful v15 driving trials remain documented in
[the collision incident report](HIGH_SPEED_RESET_INCIDENT.md). The complete
Electron-to-LMU workflow still needs a live session; the overlay live-game validation and a
standalone distributable bundling Python/native assets remain outstanding.
