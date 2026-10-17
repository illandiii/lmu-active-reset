# Sector user guide

Sector helps you practice sections of a track with **Active Reset** and keep
race information visible with **Overlay**. This guide covers everyday use.
For the download and first launch, start with [Install Sector](../README.md#-install-sector).

## Set up LMU

On first launch, confirm the LMU folder Sector finds. If no folder is found,
select the folder containing `Le Mans Ultimate.exe` yourself. You can find it
through Steam: open LMU's **Manage → Browse local files** option in your Library.

**Active Reset requires LMU's Offline mode.** To prepare the game:

1. Launch LMU **without Easy Anti-Cheat (EAC)**.
2. Select **Offline** mode in LMU.
3. Start a **Practice** session and enter **DRIVE** to drive.

Sector can remain open while you load the session; it connects automatically
when the game is ready.

For overlays, use **windowed or borderless mode** in LMU's display settings.

## Practice a corner or section

1. Drive to the point where you want your practice attempt to begin.
2. Press **F6** and wait for the successful-save confirmation.
3. Drive the section you want to practice.
4. Press **F7** to return to the saved point and try again.

The checkpoint includes the car's direction and saved driving state. A successful
new save replaces the previous checkpoint. If a save is rejected, check the
message in Sector before trying to restore.

Changing sessions or having the game rebuild your car can make a checkpoint
unavailable. Check its status on the **Active Reset** page and save a new one
when needed.

### Choose your shortcuts

Click a shortcut on the Active Reset page, then press the key you want to assign.
Press **Escape** to cancel. Use a different key for each action and avoid keys
already used by LMU. Release the keys before returning to the game.

Confirmation sounds can be previewed or muted on the same page.

## Arrange your overlays

1. Open the **Overlay** tab and enable the widgets you want.
2. Turn on **Overlays**, then select **Edit layout**.
3. Move each widget using its blue strip. Drag its lower-right corner to resize it.
4. Select **Lock layout** when you are happy with the arrangement. **Escape** also locks it.

Select a widget in Sector to change its size, transparency and available options.
Column and section switches let you remove information you do not need. For
example, hide the clutch in Inputs or remove manufacturer logos from standings.
The widget adjusts to the remaining content.

For multiple monitors, choose the display in **Edit → Settings → Overlay**.
Your layout and preferences are saved for the next launch.

**Tyres** always shows all four tyres. You can hide readings and change the
cold/hot color thresholds. Tyre laps stay
unknown until Sector observes a fresh tyre or replacement. `puncture` comes from
the game; `~flat spot` is Sector's estimate from sustained wheel locks. Increase
**Flat spot sensitivity** for earlier warnings, or hide **Tyre condition** to
remove them. The estimate clears on an observed tyre change, reset or new session.
It cannot detect damage that happened before Sector started observing the tyres.

**Stint planner** estimates remaining laps from fuel and virtual-energy consumption.
Drive complete clean laps to collect the data, then choose how many laps to average
and an optional reserve. Treat these figures as estimates that change with your pace.

**Leaderboard** keeps your position and nearby rivals inside your class section.
Driving a Hypercar puts your group in Hypercar, before the other classes. Its
compound and virtual-energy columns show the available data for each car and
can be hidden independently.

**Hybrid** shows battery charge, electric power and deploy/regen state. Set the
battery target and the number of laps to reach it. The deployment map automatically
uses your car's range: 0 to 50 in steps of 10 for LMDh, or 0 to 200 in steps of 20
for hybrid LMH. Sector learns battery changes
from clean laps with a constant map; it suggests only maps observed over at least
two laps each at a similar pace. The suggestion stays `-` until enough data exists.
It does not change the map for you. Non-hybrid cars show `N/A`.

### Samples and missing information

You can arrange widgets while LMU is closed using the editor's sample display.
Those samples are for layout editing. During play, widgets use information from
the game; unavailable values show a dash.

Lap history starts when Sector begins observing the session. Relative gaps are
estimates, marked with `~`. Some information, such as future weather forecasts,
is not available from the game reader; you can hide those sections.

## Home

The **Home** tab shows your current session, checkpoint actions and overlay
layout. **Launch LMU** opens the game from the folder chosen during setup;
keep Steam available. It does not select a game mode for you: choose **Offline**
and start **Practice** for Active Reset. Session data updates even with overlays off.

- **Save checkpoint** and **Restore checkpoint** perform the action while the game is ready in driving.
- **Open Active Reset** lets you change the assigned keys and confirmation sounds.
- **Edit layout** opens the overlay editor and unlocks the widgets.
- **Overlays on/off** uses the same setting as the Overlay tab.

## Settings, guide and background mode

- Open **Edit → Settings** for app settings and overlay display selection.
- Open **Help → Changelog** for the changes in each version, newest first.
- Enable **Reduce motion** there if you prefer fewer interface animations.

The Sector launch animation appears only when starting the app. Click or press
any key to skip it. Reopening Sector from the tray does not replay it.
- Open **Help → Guide** to replay the guided tour.
- Choose **Run in background** when closing the window to keep Sector active.
- Reopen it from the Windows notification area or by launching `Sector.exe` again.
- Choose **Quit Sector** to stop the app completely.

## Common problems

### Sector is waiting for LMU

Check that LMU is running from the folder you selected during setup. For Active
Reset, select **Offline** mode in LMU, load **Practice** without EAC and enter **DRIVE**. Keep Sector open
while the session loads, then check the status on the Active Reset page.

### The shortcut does nothing

Return focus to LMU, release both shortcut keys and press the key once. Check
that Sector is connected and that you have saved a checkpoint before restoring.
If LMU uses the same key, choose another in Sector. If Sector shows a rejection
message, use that message to identify what needs changing.

### The overlays are not visible

Check that **Overlays** and the individual widgets are enabled. Confirm the
selected display in Settings. Load a session, bring LMU to the foreground and
use windowed or borderless mode. Live widgets hide when game information is
unavailable or when another app is in the foreground.

### The widgets are catching mouse clicks

Select **Lock layout** or press **Escape** to leave layout editing. Locked
widgets let mouse input pass through to the game.

### Sector disappeared after I closed its window

If you chose **Run in background**, it is still running. Look for its icon beside
the Windows clock, including inside the hidden-icons menu, or launch Sector again.
Choose **Quit Sector** in the close dialog when you want to stop it.

### I cannot find Sector.exe after downloading

Make sure you downloaded the Windows app ZIP from the release's **Assets**, then
extracted it. GitHub's **Source code** ZIP is for developers and does not contain
the ready-to-use application. If no Windows package is attached, one has not been
published on that release yet.

## Update Sector

1. Close Sector using **Quit Sector**.
2. Download the new Windows package and extract it into a new folder.
3. Launch `Sector.exe` from that folder and update any desktop shortcut you use.

Keep all files in the new package together. Your settings and first-run choices
are stored separately and are retained between versions.

## Get more help

Open an [issue on GitHub](https://github.com/sector-lmu/Sector/issues) with the
Sector version you are using, what you were doing and the exact message shown
by the app. A screenshot can help explain a display problem.

Development and build instructions are in [Contributing](../CONTRIBUTING.md).
