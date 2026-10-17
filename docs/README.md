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

### Samples and missing information

You can arrange widgets while LMU is closed using the editor's sample display.
Those samples are for layout editing. During play, widgets use information from
the game; unavailable values show a dash.

Lap history starts when Sector begins observing the session. Relative gaps are
estimates, marked with `~`. Some information, such as future weather forecasts,
is not available from the game reader; you can hide those sections.

## Settings, guide and background mode

- Open **Edit → Settings** for app settings and overlay display selection.
- Enable **Reduce motion** there if you prefer fewer interface animations.
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
