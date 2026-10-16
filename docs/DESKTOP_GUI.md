# Sector desktop UI and continuous host v15

## Current integration

The `codex/desktop-gui` design branch (`8d166d1`) is already an ancestor of main.
Sector retains its WPF design and connects its settings and status presentation
to the v15 host used in the successful live resets. Open `Start-Sector.cmd`;
`Start-Reset.cmd` remains compatible. No onboarding or overlay is added here.

The latest layout has pinned Active Reset and Overlay tabs, a themed scrollbar,
and compact status/shortcut/audio cards. Overlay currently displays Coming soon.
The Open logs footer and explanatory filler text have been removed. Runtime
errors remain visible. Four matching SVG source icons in `assets/icons` use the
same geometry as WPF Paths, so they scale without a browser or SVG dependency.

Shortcut buttons open an embedded capture card. The window consumes the next
supported key, saves it atomically and updates the running controller through
the existing settings channel. Escape cancels, duplicates stay in capture, and
focus loss cancels capture. Modifiers and combinations are rejected. UI and Python
share `src/reset_keymap.json`, including letters, digits, F1–F24, navigation,
numpad and OEM punctuation. Punctuation labels use the active keyboard layout.
Existing F6/F7 preferences remain valid. Key release and game-focus guards still
apply before any native command is admitted.

The checkpoint card reads acknowledged state from `status.json`: saved speed,
successful restores since the last save, connection phase and the last result.
Pressing a key or completing preparation never increments the restore count.
A rejected replacement save retains the previous checkpoint and its summary.
Vehicle invalidation clears the summary. Settings still live in
`%LOCALAPPDATA%/LMUActiveReset/settings.json` to preserve existing preferences.

Closing the window offers background, quit and cancel. Background hides the
window while the dispatcher, heartbeat and hotkey process continue running.
The tray provides Open Sector and Quit Sector. A named mutex allows only one
desktop controller per Windows session; another launch signals the existing
window to reopen. Quit requests cooperative host shutdown and waits for the
child to exit before disposing the tray and application. No game process is killed.
The close prompt is a modal layer inside the main window, not another native
window. Its focus stays inside the prompt while the controller heartbeat continues.

`ResetGui.Status.ps1` contains presentation logic, `ResetGui.Instance.ps1` owns
the desktop instance lease, and `ResetGui.SmokeTest.ps1` contains isolated window
tests. The Python `DesktopSession` publishes a versioned snapshot (`schema_version`
1) through atomic replacement. A future overlay can consume that read-only state
without becoming a second reset controller. It must not create or refresh the
desktop heartbeat, grant native admission or infer success from keypresses.

Additional WPF smoke checks cover actual routed keyboard events, duplicate and
modifier rejection, Escape cancellation, switching tabs, embedded close/cancel,
and scrollbar position synchronization at the minimum window height. Main, Overlay,
capture and close states were rendered and inspected. These checks run in Preview
mode and never connect to LMU or modify the user's saved preferences.

Windows validation: 92 Python unit tests, Ruff checks and formatting, settings
persistence, process-to-process instance exclusion/reopen signaling, WPF loading,
background/quit choices, hide/reopen and status rendering all pass. The layout
was rendered and inspected locally. The original seven live reset trials used
the hotkey launcher; this desktop/tray integration has not repeated live driving.

The remaining sections preserve the original design and validation history;
their historical pending items are superseded where the current results above
or the collision incident report provide later evidence.

## Product and design

The selected direction is the Apple-inspired compact utility, translated to native
Windows WPF. This is an English, dark-only window with a checkpoint summary,
two function-key selectors and sound controls. The user explicitly removed the
session countdown, start/stop buttons, focus reminder and experimental-status copy
from the window. The existing high-speed physics defect is documented in the
repository and is not fixed by this UI change.

The design uses the requested [Apple design skill](https://github.com/dickwu/apple-design-skill):
its HIG references `layout.md › Visual hierarchy` (“Group related items”),
`feedback.md › Best practices` (“Consider integrating status feedback into your
interface”), and `settings.md › Best practices` (“Minimize the number of settings”).
Windows window chrome, keyboard interaction and Segoe UI remain native.

Semantic colors: surface #1C1C1E, cards #2C2C2E, text #F5F5F7,
secondary text #B0B0B8, accent #73B7FF. Windows high-contrast colors take precedence.
The content follows Windows text scaling, scrolls at small sizes, and stacks sound
previews when the window is narrow. Controls expose accessible names and focus states.
There is no animation, transparency or additional theme switch.

## Startup and settings

1. On the already configured Windows machine, close LMU and Active Reset.
2. Run `pwsh -File scripts/Build-ResetDesktop.ps1 -ZigPath C:\tools\zig\zig.exe`.
   Supply `-FrameDirectory <recorded-fixture-directory>` to also run the full
   host fixtures with the local numerical frame.
3. Open `Start-Reset.cmd`. LMU can start before or after the app.
4. The window detects one LMU process and waits for offline Practice driving.
   F6 saves and F7 restores by default. No reset is performed automatically.

The build uses the existing local
`compatibility/steam-25661166-telemetry.json` and the installed game SDK.
It verifies pinned game files, builds/tests telemetry, runs the core suites,
builds the new host, updates the diagnostic hash and enables continuous capture
in that local profile. A clean public clone has neither the installation manifest
nor captured frames; those cannot be reconstructed from repository contents.

Settings live in `%LOCALAPPDATA%/LMUActiveReset/settings.json`. Duplicate keys
and values outside F1–F24 are rejected inline. Valid changes are saved atomically.
Key changes apply once no command is in flight, disarm the previous edge and
require a fresh key release. They preserve the checkpoint. Sound feedback can
be muted without restarting the host.

Sounds are short locally generated WAVs, played asynchronously after a native
acknowledgement. Save, restore and rejection have different patterns. Prepare
success does not play a restore sound. Pressing restore before any save produces
a rejection sound and an inline explanation. The UI also displays command results
for users who mute audio. “Checkpoint applied” describes state application, not a
claim that the unresolved post-reset physics instability is fixed.

## Lifetime

The four-minute runner limit and five-minute native limit are removed. The
telemetry child accepts duration zero and runs until it is stopped or LMU exits.
There is no checkpoint-age test. A failed save preserves the previous checkpoint.

The existing lifecycle hooks invalidate a checkpoint when the vehicle is rebuilt
or its identity changes, including pit reconstruction at the same address. The
worker observes the changed epoch without waiting for another keypress, publishes
`HOST_STOP_LIFECYCLE` in the existing reserved IPC field, and detaches normally.
The supervisor audits stopped hooks and waits for stable driving before creating
a new instance. It never revives the old checkpoint.

The existing lifecycle guard is conservative: its registered mutation entries
also cover other vehicle/control changes, not solely a semantic “return to pits”
button. This change does not weaken that guard or claim complete live coverage.
Context changes out of offline Practice invalidate the active connection.
Focus loss and a monitor observation alone pause admission without deleting a
checkpoint; a native reconstruction during that pause still invalidates it.

Controller-liveness watchdogs, HTTP deadlines, request freshness and native
prepare/restore acknowledgement windows remain. They detect abandoned controllers
and stale operations; they are not a checkpoint/session expiry. Closing the GUI
requests stop and waits for cleanup. If the GUI is killed, its heartbeat expires
and the child requests native stop. Unexpected/quarantined failures are displayed
and are not automatically retried against the same game instance.

Stopped DLLs remain resident until the game exits, as before. Long sessions retain
diagnostic logs. V14 keeps a bounded 512-record ring in game memory and drains it
to disk. It no longer stops recording after the first 512 samples. Sequence stamps
reject records overwritten during a read; slow-reader losses are recorded as
explicit gaps. The analyzer never pairs state across those gaps. Disk logs grow
with the session; the native ring size stays constant.

## Verification in the Linux workspace

- 29 Python tests passed: hotkey sequencing, indefinite checkpoint retention,
  rebind/release behavior, failed-save preservation, lifecycle re-admission,
  no retry after unexpected failure, GUI shutdown lease, audio, and trace analysis.
- PowerShell settings tests passed: defaults, persistence, atomic replacement,
  duplicate/range validation and malformed-file recovery. XAML is well-formed;
  PowerShell source parses.
- Portable native fixtures passed: controller liveness after 30 simulated days,
  and restoring a checkpoint with a fresh request one simulated day later.
- Zig 0.16.0 cross-compiled the v13 Windows DLL and the complete host fixture
  DLL/executable successfully.
- `Test-Core.ps1` was invoked. The new policy suite passed; the legacy transaction
  build then stopped on Windows-only backslash source paths in this Linux host.
  The full Windows suite has **not** passed here.
- The updated native lifecycle fixture and full host fixture were compiled, but
  cannot execute here without Windows and the local recorded numerical frame.
- WPF layout/rendering, screen-reader behavior, live sound output and LMU automatic
  attachment/pit-reset cycles still require Windows validation. No LMU reset was
  performed during this work.

Windows checks:

```powershell
./scripts/Test-ResetGui.ps1 -IncludeWpf
./scripts/Start-ResetGui.ps1 -Preview
./scripts/Test-Core.ps1 -ZigPath C:\tools\zig\zig.exe
```

Then check launch-before-game, launch-after-game, F6/F7, duplicate/reassigned keys,
audio previews/mute, focus changes, more than five minutes of continuous operation,
pit reconstruction and close/reopen. Use the existing collision test protocol for
any moving-reset validation; this change does not establish high-speed stability.

## Windows validation on 16 October 2026

The desktop branch was integrated locally and `Build-ResetDesktop.ps1` completed
on Windows with the recorded fixture frame. Telemetry schema/self-tests, all
`Test-Core.ps1` suites, the v13 full restore/absent-player/restart fixtures and WPF
settings/layout validation passed. This supersedes the Linux execution limitations
above for those checks, but does not establish live-game or visual/audio behavior.

V14 was then built and its full host fixtures passed, including all 10,077 writes,
four native-contact fixture calls, lifecycle stop and two-instance restart. Its
dedicated trace fixture published 1,555 samples across multiple ring wraps and
checked the 704,520-byte mapping layout. Forty Python tests passed, covering
continuous draining, exact loss counts, concurrent slot reuse, a crash during reuse,
legacy trace decoding and analysis boundaries. V14 is the default local launcher
version. LMU was not started or attached during these checks.

At that stage closing the window disconnected and exited, background/tray work
was pending, and the high-speed reset defect was still open. See the current
integration above and `HIGH_SPEED_RESET_INCIDENT.md` for the later v15 results.
