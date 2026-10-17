# Telemetry overlays

Sector provides ten widgets based on the supplied `LMU Overlay.html` and
`LMU Overlay - 2.html` designs: **Leaderboard, Relative, Weather, Delta, Inputs,
Laps, Session, Brake lock, Tyres and Stint planner**. Artwork and
fonts are bundled locally; the exported page's remote runtime is not executed.

## Configure the HUD

1. Open **Overlay**, choose the widgets and enable **Overlays**.
2. Select a widget to change position, size, opacity and its available options.
3. Choose a display through **Edit → Settings → Overlay**.
4. Select **Edit layout**, drag the blue strip or resize from the lower-right corner.
5. Select **Lock layout**, the overlay check button or Escape to return mouse input to LMU.

Dragging and resizing update the windows while the pointer moves. Valid Size
field changes also apply immediately; the editor preview may be fitted to its
available space. Edit/Lock uses a fixed-width button. Background mode locks the
layout automatically.

Column and section switches are arranged in two columns. Hidden table columns,
including manufacturer logos, remove their grid tracks and shrink the widget.
The live leaderboard omits classes with no entries. Accent controls are available
only where used: the Inputs steering wheel and the Session position section.

Preferences persist in `%LOCALAPPDATA%/LMUActiveReset/overlays.json`, independently
of reset shortcuts. Display changes keep widget geometry within accessible bounds.

## Widgets and data

| Widget | Data and customization |
| --- | --- |
| Leaderboard | Scored positions and classifications; row, column and logo options; absent classes hidden |
| Relative | Nearby cars and estimated relative gaps; configurable rows and columns |
| Weather | Current SDK weather; independently hide unavailable forecast sections |
| Delta | Player delta from telemetry; configurable visible sections |
| Inputs | Pedals, input trace, steering, gear and speed; clutch can be hidden; steering rotation adjustable |
| Laps | Observed lap history and supported usage statistics; configurable columns and rows |
| Session | Session and player information; individually configurable sections |
| Brake lock | Per-wheel slip estimate under braking, recent braking events and an observed pedal threshold; configurable slip threshold and minimum speed |
| Tyres | Remaining tyre life, three tread-temperature bands, axle compound names, observed tyre age, punctures and estimated flat spots; each wheel and reading can be hidden |
| Stint planner | Fuel/virtual-energy range, conservative pit lap, measured consumption and energy target for an extra lap; adjustable averaging window and lap reserve |

Brake lock compares each loaded wheel's rotational speed and deflected radius
against its own longitudinal ground speed. It ignores detached, airborne and
off-road wheels, and requires braking above the selected minimum speed. The
`~` pedal threshold summarizes recent detected events; it is an estimate, not a
fixed limit valid for every corner. The slip threshold defaults to 20% and the
minimum speed to 20 km/h. The wheel diagram shows the current result.

Tyre temperatures are converted from Kelvin to Celsius. The three bands retain
the SDK's left/centre/right order. Tyre life is the SDK remaining fraction; color
thresholds are display preferences, not compound-specific optimal temperatures.
Tyre laps are counted after an observed fresh tyre or compound replacement and
remain unknown when attaching mid-stint. `puncture` uses the SDK's real puncture flag.

Sector estimates flat spots itself and marks them `~flat spot`. It tracks loaded,
attached wheels braking above 20 km/h on tarmac or kerbs, with at least 80% slip
and near-zero rotation. Sliding distance, duration and load accumulate on a small
tread patch; rotation beyond 30 degrees starts a different patch. At least 0.2
seconds of continuous evidence is required. Duplicated timestamps add nothing,
and gaps over 150 ms interrupt the event. Separate brief lockups are not added
together. These conservative rules can miss short locks or pre-existing damage.

**Flat spot sensitivity** ranges from 10 to 100 (default 50). Higher values lower
the warning threshold. Internally, the default threshold is 40 equivalent sliding
metres at 3 kN, scaled inversely with sensitivity. This is an uncalibrated heuristic,
not a measurement of LMU's tyre deformation. The warning persists after braking
and clears on an observed tyre replacement, reset or session/vehicle change.
Hide **Tyre condition** to remove both the tread marker and warning text.

Stint planner averages the most recent complete clean laps (five by default).
Pit and invalid laps are excluded, and zero or missing consumption is not treated
as unlimited range. Both fuel and virtual-energy measurements are needed for the
combined pit estimate. A reserve subtracts whole laps from that estimate. The
extra-lap target refers to virtual-energy usage; fuel can still be the limiting
resource. Predictions assume the observed consumption continues and round down
to complete laps. Reset acknowledgements clear these histories, including short
rewinds that do not cross a lap boundary.

Relative time gaps are distance-based estimates marked `~`, not timing-loop
measurements. Lap differences remain the scored lap differences. Lap history
starts when Sector observes the session. Partial first laps, pit laps and invalid
laps do not become clean statistical samples; fuel and energy usage require both
lap boundaries. Vehicle/session changes, backward clocks or laps, and detected
position discontinuities reset history.

Humidity, future forecasts, exact grip percentage and rain-arrival predictions
are not supplied by this reader and display a dash. Car numbers are shown only
when explicitly present as `#number` in the SDK vehicle name. Unknown values are
never replaced with sample telemetry.

## Visibility and input

Each widget is a transparent, frameless, topmost Electron window. Locked windows
are non-focusable and ignore mouse input. They appear only with fresh telemetry,
a loaded session containing cars and LMU as the foreground process. This includes
the pit monitor when the session is loaded. Stale telemetry or leaving LMU hides
the live HUD; disabled widgets release their windows.

Use LMU in **windowed or borderless mode**. These are external desktop windows,
not an injected exclusive-fullscreen overlay. Live behavior across the user's
display setup still requires in-game validation.

The layout editor can show clearly labelled samples with LMU closed. Locked live
widgets never substitute those examples for missing data. Manufacturer artwork
uses the transparent variant of the supplied logo sheet; see [notices](../src/ui/electron/THIRD_PARTY_NOTICES.md).

## Reader architecture

`src/native/telemetry/overlay_reader.cpp` consumes the locally verified LMU SDK
layout for build 14200. Game data is mapped read-only; the SDK synchronization
words are the only writable mapping. The reader acquires the nonblocking SDK
lock, copies bounded arrays and serializes outside the lock. It does not inject
code or issue reset commands.

The controller bounds JSON buffering, expires frozen scoring/player timestamps
after 1.5 seconds and clears frames on disconnect. The reader exits when its
controller pipe closes or its owner/game terminates. The desktop does not kill
it while it might own the SDK lock. Isolated preview starts neither this reader
nor the reset host.

## Build and verification

```powershell
./scripts/build/Build-OverlayReader.ps1 -ZigPath <zig.exe> -SdkPath <LMU-SDK-directory>
```

The desktop build also invokes this reader build. SDK headers and binaries are
local prerequisites and are not committed; layout assertions reject incompatible
SDK builds.

`npm test` checks preference validation, bounds, stale/missing telemetry, lap
history and shutdown races. `npm run test:telemetry` passes the compiled reader's
synthetic SDK frame through the production widget models. `npm run test:ui` checks all ten designs, columns,
clutch and logo removal, live resize before pointer release, display selection,
fixed button widths, window isolation and foreground gating. These fixture and
desktop checks do not replace live-game qualification. See [test coverage](TEST_RESULTS.md).
