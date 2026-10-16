# Sector overlays

The seven widgets reproduce the supplied `LMU Overlay.html`: Leaderboard,
Relative, Weather, Delta, Inputs, Laps and Session. The original inline layout,
font files, SVG artwork and colors are preserved in `renderer/overlays/designs`.
`logos.png` is the supplied sheet, shown through SVG viewports without redrawing
or modifying the brand artwork. The font license accompanies the bundled files.
The exported page's runtime and remote resources are not executed or required.

## Use

In **Overlay**, enable the widgets you want, then enable **Overlays**. Select a
widget to change X/Y, size, opacity, accent and row count. Inputs can independently
hide clutch, trace, steering wheel and gear/speed; steering rotation is adjustable.
Classifications can hide manufacturer logos. Values persist separately from reset
hotkeys at `%LOCALAPPDATA%/LMUActiveReset/overlays.json`.

Choose a display and **Edit layout** to drag each window by its blue strip or resize
it by its lower-right corner. **Lock layout**, its check button or Escape releases
mouse input to LMU. Background mode also locks the layout. Editor examples carry a
**Sample** label; a locked live HUD never displays example telemetry.

Each widget is a transparent, frameless, topmost Electron window. Locked windows
are non-focusable and ignore mouse input. Live widgets appear only while telemetry
is fresh, the player is driving and LMU is the foreground process. Disabled widgets
release their windows. Display changes clamp geometry to accessible bounds.
Use LMU in windowed/borderless mode: an external desktop window is not an injected
exclusive-fullscreen overlay. This behavior still needs a live-game check on the
user's display configuration.

## Telemetry and limits

`overlay_reader.cpp` consumes the installed LMU SDK shared-memory layout for
version 14200. `LMU_Data` is mapped read-only; the only writable mapping is the
SDK's existing synchronization words. The reader uses the same nonblocking lock
protocol as the diagnostic, copies bounded arrays, then serializes outside the
lock. It neither injects into LMU nor sends reset commands. It exits on controller
pipe closure or owner/game termination. The desktop never forcibly kills it while
it might own the SDK lock.

The controller retries discovery, bounds JSON buffering, expires frozen scoring
or player timestamps after 1.5 seconds, and clears frames on disconnect. Preview
mode cannot start either the telemetry reader or the reset host.

- Inputs, speed, gear, delta, current weather, fuel, virtual energy, lap times,
  positions, pit count and track-limit steps use SDK fields.
- Relative gaps are distance-based estimates, prefixed with `~`; lap difference
  remains the scored lap difference. They are not timing-loop measurements.
- Lap history starts when Sector observes the session. A partial first lap, pit
  lap or invalid lap cannot become a clean statistical sample. Fuel/energy usage
  requires both lap boundaries. History resets on vehicle/session changes,
  backward timestamps/laps or detected position discontinuities.
- Humidity, future weather, exact grip percentage and rain-arrival predictions
  are not supplied by this reader and show a dash. Car number is shown only when
  explicitly present as `#number` in the SDK vehicle name. Unknown data is never
  replaced by a demonstration value.
- The seven example layouts remain available for editing when LMU is closed.

## Build and checks

The existing configured machine has the generated reader in `bin/`. Rebuild with:

```powershell
./scripts/Build-OverlayReader.ps1 -ZigPath <path-to-zig.exe> -SdkPath <LMU-SDK-directory>
```

`Build-ResetDesktop.ps1` also builds this reader. Proprietary SDK headers and
compiled binaries are not committed. The SDK's static layout checks intentionally
reject an incompatible build.

`npm test` covers preference validation/persistence, screen bounds, real versus
missing values, lap-history discontinuities, stale frames and shutdown races.
`npm run test:ui` runs the existing desktop regression plus the overlay suite:
all widget designs, logo assets, controls, clutch removal, independent sandboxed
windows, movement persistence, locking, foreground gating and compact layouts.
Native `--self-test` checks array bounds and scoring/telemetry player identity.
These are fixture and desktop tests, not a live-game qualification.
