param(
    [string]$PythonPath,
    [switch]$Preview,
    [switch]$ValidateOnly,
    [string]$PreviewImage,
    [switch]$SmokeTest,
    [ValidateSet("Reset", "Overlay", "Capture", "Close")][string]$PreviewState = "Reset"
)
$ErrorActionPreference = 'Stop'
if ($SmokeTest -and -not $Preview) { throw 'SmokeTest requires Preview; it never connects to LMU.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'ResetGui.Settings.ps1')
. (Join-Path $PSScriptRoot 'ResetGui.Status.ps1')
. (Join-Path $PSScriptRoot 'ResetGui.Instance.ps1')
if ($SmokeTest) { . (Join-Path $PSScriptRoot 'ResetGui.SmokeTest.ps1') }
Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ResetWindowTheme {
    [DllImport("user32.dll")]
    public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("dwmapi.dll")]
    public static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
}
'@

$reader = [Xml.XmlReader]::Create((Join-Path $projectRoot 'src/ui/reset_gui.xaml'))
try { $window = [Windows.Markup.XamlReader]::Load($reader) } finally { $reader.Close() }
$window.Add_SourceInitialized({
    try {
        $handle = [Windows.Interop.WindowInteropHelper]::new($window).Handle
        $dark = 1
        [ResetWindowTheme]::DwmSetWindowAttribute($handle, 20, [ref]$dark, 4) | Out-Null
    } catch { } # Older Windows still renders the dark content with native chrome.
})
$ui = @{}
foreach ($name in @('LayoutRoot', 'MainContent', 'BrandMark', 'StatusDot', 'StatusTitle', 'CheckpointState', 'CheckpointSummary', 'LastEvent',
    'SaveKey', 'RestoreKey', 'SoundEnabled', 'PreviewSave', 'PreviewRestore', 'SoundPreviews', 'SettingsHint',
    'ResetTab', 'OverlayTab', 'ResetPanel', 'OverlayPanel', 'ModalLayer', 'CaptureCard', 'CloseCard',
    'CaptureTitle', 'CaptureHint', 'CancelCapture', 'KeepRunning', 'QuitSector', 'CancelClose')) {
    $ui[$name] = $window.FindName($name)
    if ($null -eq $ui[$name]) { throw "Missing UI element: $name" }
}
$logoPath = Join-Path $projectRoot 'assets/sector-logo.png'
$ui.BrandMark.Source = [Windows.Media.Imaging.BitmapImage]::new([uri]$logoPath)
$window.Icon = [Windows.Media.Imaging.BitmapImage]::new([uri](Join-Path $projectRoot 'assets/sector.ico'))
if ($ValidateOnly) {
    if ($PreviewState -eq 'Overlay') {
        $ui.OverlayTab.IsChecked = $true
        $ui.ResetPanel.Visibility = 'Collapsed'
        $ui.OverlayPanel.Visibility = 'Visible'
    }
    if ($PreviewState -in @('Capture', 'Close')) {
        $ui.ModalLayer.Visibility = 'Visible'
        $ui.MainContent.IsEnabled = $false
        if ($PreviewState -eq 'Capture') { $ui.CaptureCard.Visibility = 'Visible' }
        else { $ui.CloseCard.Visibility = 'Visible' }
    }
    $window.Measure([Windows.Size]::new(580, 660))
    $window.Arrange([Windows.Rect]::new(0, 0, 580, 660))
    $window.UpdateLayout()
    if ($PreviewImage) {
        $window.Content.Measure([Windows.Size]::new(580, 660))
        $window.Content.Arrange([Windows.Rect]::new(0, 0, 580, 660))
        $window.Content.UpdateLayout()
        $visual = [Windows.Media.DrawingVisual]::new()
        $drawing = $visual.RenderOpen()
        $drawing.DrawRectangle($window.Resources['Surface'], $null, [Windows.Rect]::new(0, 0, 580, 660))
        $drawing.Close()
        $bitmap = [Windows.Media.Imaging.RenderTargetBitmap]::new(580, 660, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
        $bitmap.Render($visual)
        $bitmap.Render($window.Content)
        $encoder = [Windows.Media.Imaging.PngBitmapEncoder]::new()
        $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
        $output = [IO.File]::Create($PreviewImage)
        try { $encoder.Save($output) } finally { $output.Dispose() }
    }
    Write-Output 'PASS: WPF window, named controls, shortcut buttons and layout loaded.'
    return
}

# Keep the requested dark appearance; use Windows contrast colors when requested.
function Set-AccessibilityAppearance {
    if ([Windows.SystemParameters]::HighContrast) {
        $window.Resources['Surface'] = [Windows.SystemColors]::WindowBrush
        $window.Resources['Card'] = [Windows.SystemColors]::WindowBrush
        $window.Resources['Content'] = [Windows.SystemColors]::WindowTextBrush
        $window.Resources['Secondary'] = [Windows.SystemColors]::WindowTextBrush
        $window.Resources['Accent'] = [Windows.SystemColors]::HighlightBrush
        $window.Resources['Primary'] = [Windows.SystemColors]::HighlightBrush
        $window.Resources['OnPrimary'] = [Windows.SystemColors]::HighlightTextBrush
        $window.Resources['Line'] = [Windows.SystemColors]::WindowTextBrush
        $window.Resources['ControlBorder'] = [Windows.SystemColors]::WindowTextBrush
        $window.Resources['Hover'] = [Windows.SystemColors]::ControlBrush
    }
    $textScale = 100
    try {
        $setting = Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Accessibility' -Name TextScaleFactor -ErrorAction Stop
        $textScale = [Math]::Max(100, [Math]::Min(225, [double]$setting.TextScaleFactor))
    } catch { }
    $ui.LayoutRoot.LayoutTransform = [Windows.Media.ScaleTransform]::new($textScale / 100, $textScale / 100)
}
Set-AccessibilityAppearance
$ui.LayoutRoot.Add_SizeChanged({
    $ui.SoundPreviews.Columns = if ($ui.LayoutRoot.ActualWidth -lt 390) { 1 } else { 2 }
    if ($ui.SoundPreviews.Columns -eq 1) {
        $ui.PreviewSave.Margin = [Windows.Thickness]::new(0, 0, 0, 6)
        $ui.PreviewRestore.Margin = [Windows.Thickness]::new(0)
    } else {
        $ui.PreviewSave.Margin = [Windows.Thickness]::new(0, 0, 6, 0)
        $ui.PreviewRestore.Margin = [Windows.Thickness]::new(6, 0, 0, 0)
    }
})

$settingsPath = Join-Path $env:LOCALAPPDATA 'LMUActiveReset/settings.json'
$settings = Read-ResetSettings $settingsPath
$ui.SaveKey.Tag = $settings.save_key
$ui.SaveKey.Content = (Get-ResetKey $settings.save_key).label
$ui.RestoreKey.Tag = $settings.restore_key
$ui.RestoreKey.Content = (Get-ResetKey $settings.restore_key).label
$ui.SoundEnabled.IsChecked = $settings.sound
$script:preferences = $settings
$script:child = $null
$script:sessionPath = $null
$script:lastSequence = -1
$script:lastSnapshot = $null
$script:blockedIdentity = $null
$script:closingRequested = $false
$script:canClose = $false
$script:nextDiscovery = [datetime]::MinValue
$script:nextHeartbeat = [datetime]::MinValue
$script:startupError = $null
$audioDirectory = Join-Path $projectRoot 'work/ui-audio'
$script:audioPlayer = $null
$script:tray = $null
$script:showRequest = $null
$script:instance = $null

# Reopening the launcher reveals the running app instead of creating another host.
if (-not $Preview) {
    $script:instance = Open-ResetGuiInstance
    $script:showRequest = $script:instance.ShowRequest
    if (-not $script:instance.IsOwner) {
        Close-ResetGuiInstance $script:instance
        return
    }
}

function Save-Preferences {
    $candidate = [pscustomobject]@{
        save_key = [string]$ui.SaveKey.Tag
        restore_key = [string]$ui.RestoreKey.Tag
        sound = [bool]$ui.SoundEnabled.IsChecked
    }
    if (-not (Test-ResetSettings $candidate)) {
        $ui.SettingsHint.Text = 'Choose different keys.'
        $ui.SettingsHint.Visibility = 'Visible'
        return $false
    }
    try {
        if (-not $Preview) { Save-ResetSettings $settingsPath $candidate }
        $script:preferences = $candidate
        $ui.SettingsHint.Text = ''
        $ui.SettingsHint.Visibility = 'Collapsed'
        $ui.PreviewSave.IsEnabled = $candidate.sound
        $ui.PreviewRestore.IsEnabled = $candidate.sound
        return $true
    } catch {
        $ui.SettingsHint.Text = 'Settings could not be saved'
        $ui.SettingsHint.ToolTip = $_.Exception.Message
        $ui.SettingsHint.Visibility = 'Visible'
        return $false
    }
}
$ui.SoundEnabled.Add_Checked({ Save-Preferences | Out-Null })
$ui.SoundEnabled.Add_Unchecked({ Save-Preferences | Out-Null })
Save-Preferences | Out-Null
. (Join-Path $PSScriptRoot 'ResetGui.Interactions.ps1')
Initialize-ResetInteractions

function Play-Preview([string]$Name) {
    $path = Join-Path $audioDirectory "$Name.wav"
    if (-not [IO.File]::Exists($path)) {
        $ui.SettingsHint.Text = 'Sound preview is not ready'
        $ui.SettingsHint.Visibility = 'Visible'
        return
    }
    try {
        if ($script:audioPlayer) { $script:audioPlayer.Dispose() }
        $script:audioPlayer = [Media.SoundPlayer]::new($path)
        $script:audioPlayer.Play()
    } catch {
        $ui.SettingsHint.Text = 'Audio device unavailable'
        $ui.SettingsHint.Visibility = 'Visible'
    }
}
$ui.PreviewSave.Add_Click({ Play-Preview 'save' })
$ui.PreviewRestore.Add_Click({ Play-Preview 'restore' })

function Set-Status([string]$Title, [string]$Detail, [string]$Tone = 'Secondary') {
    $ui.StatusTitle.Text = $Title
    $ui.StatusTitle.ToolTip = $Detail
    if ($Tone -eq 'Signal') { $ui.LastEvent.Text = $Detail; $ui.LastEvent.Visibility = 'Visible' }
    $ui.StatusDot.SetResourceReference([Windows.Shapes.Shape]::FillProperty, $Tone)
}
function Read-SessionStatus {
    if (-not $script:sessionPath) { return }
    $path = Join-Path $script:sessionPath 'status.json'
    try { $snapshot = [IO.File]::ReadAllText($path) | ConvertFrom-Json }
    catch { return } # The writer uses atomic replacement; retry transient read contention.
    if ($snapshot.sequence -eq $script:lastSequence) { return }
    $script:lastSequence = $snapshot.sequence
    $script:lastSnapshot = $snapshot
    $view = Get-ResetStatusView $snapshot
    $ui.CheckpointState.Text = $view.Checkpoint
    $ui.CheckpointSummary.Text = $view.Summary
    $ui.CheckpointSummary.Visibility = if ($snapshot.checkpoint) { 'Visible' } else { 'Collapsed' }
    if ($view.Message) { $ui.LastEvent.Text = $view.Message; $ui.LastEvent.Visibility = 'Visible' }
    Set-Status $view.Title $view.Detail $view.Tone
}

function Request-Stop {
    if ($script:child -and -not $script:child.HasExited) {
        [IO.File]::WriteAllText((Join-Path $script:sessionPath 'stop'), '')
        Set-Status 'Disconnecting' 'Finishing the current operation.'
    }
}

function Show-SectorWindow {
    $window.Show()
    $window.WindowState = [Windows.WindowState]::Normal
    $window.Activate() | Out-Null
}

function Request-Exit {
    $script:closingRequested = $true
    if ($script:child -and -not $script:child.HasExited) {
        Show-SectorWindow
        Request-Stop
    } else {
        $script:canClose = $true
        $window.Close()
    }
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$script:tray = [Windows.Forms.NotifyIcon]::new()
$script:tray.Icon = [Drawing.Icon]::new((Join-Path $projectRoot 'assets/sector.ico'))
$script:tray.Text = 'Sector'
$trayMenu = [Windows.Forms.ContextMenuStrip]::new()
$openItem = $trayMenu.Items.Add('Open Sector')
$openItem.Add_Click({ Show-SectorWindow })
$quitItem = $trayMenu.Items.Add('Quit Sector')
$quitItem.Add_Click({ Request-Exit })
$script:tray.ContextMenuStrip = $trayMenu
$script:tray.Add_DoubleClick({ Show-SectorWindow })
$script:tray.Visible = $true

if (-not $Preview) {
    try {
        if (-not $PythonPath) {
            $bundled = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
            if (Test-Path -LiteralPath $bundled) { $PythonPath = $bundled }
            else { $PythonPath = (Get-Command python -ErrorAction Stop).Source }
        }
        [IO.Directory]::CreateDirectory($audioDirectory) | Out-Null
        $audioArgs = '"{0}" desktop.reset_feedback "{1}"' -f (Join-Path $projectRoot 'scripts/sector.py'), $audioDirectory
        $script:audioProcess = Start-Process $PythonPath -ArgumentList $audioArgs -WindowStyle Hidden -PassThru
    } catch {
        $script:startupError = 'Python is unavailable. Install Python 3.11+ or pass -PythonPath.'
        Set-Status 'Setup required' $script:startupError 'Signal'
    }
} else {
    $window.Title = 'Sector - Preview'
    Set-Status 'Waiting for LMU' 'The app connects automatically when you start driving.'
}

$timer = [Windows.Threading.DispatcherTimer]::new()
$timer.Interval = [timespan]::FromMilliseconds(250)
$script:smokeStep = 0
$script:smokeFailure = $null
$timer.Add_Tick({
    try {
        if ($SmokeTest) {
            Invoke-ResetGuiSmokeStep
            return
        }
        if ($script:showRequest -and $script:showRequest.WaitOne(0)) { Show-SectorWindow }
        if ($Preview) { return }
        if ($script:child) {
            if (-not $script:child.HasExited -and [datetime]::UtcNow -ge $script:nextHeartbeat) {
                [IO.File]::WriteAllText((Join-Path $script:sessionPath 'heartbeat'), '')
                $script:nextHeartbeat = [datetime]::UtcNow.AddSeconds(1)
            }
            Read-SessionStatus
            if ($script:child.HasExited) {
                $exitCode = $script:child.ExitCode
                $script:child.Dispose()
                $script:child = $null
                $ui.CheckpointState.Text = 'No checkpoint'
                $ui.CheckpointSummary.Visibility = 'Collapsed'
                if ($exitCode -ne 0 -and $script:lastSnapshot.phase -ne 'error') {
                    Set-Status 'Unable to connect' 'Open logs for details. Reopen Sector after fixing the issue.' 'Signal'
                }
                if ($script:closingRequested) {
                    $script:canClose = $true
                    $window.Close()
                }
            }
            return
        }
        if ($script:closingRequested -or $script:startupError) { return }
        if ([datetime]::UtcNow -lt $script:nextDiscovery) { return }
        $script:nextDiscovery = [datetime]::UtcNow.AddSeconds(1)
        $games = @(Get-Process -Name 'Le Mans Ultimate' -ErrorAction SilentlyContinue)
        if ($games.Count -ne 1) {
            $script:blockedIdentity = $null
            Set-Status 'Waiting for LMU' $(if ($games.Count -gt 1) { 'Keep one LMU instance open.' } else { 'The app connects automatically when LMU starts.' })
            $ui.LastEvent.Visibility = 'Collapsed'
            return
        }
        $gameIdentity = "$($games[0].Id):$($games[0].StartTime.ToUniversalTime().Ticks)"
        if ($gameIdentity -eq $script:blockedIdentity) { return }
        $script:blockedIdentity = $gameIdentity
        $script:sessionPath = Join-Path $projectRoot ('work/gui/' + [guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($script:sessionPath) | Out-Null
        [IO.File]::WriteAllText((Join-Path $script:sessionPath 'heartbeat'), '')
        $arguments = @(
            (Join-Path $projectRoot 'scripts/sector.py'), 'runtime.start_reset_hotkeys', '--pid', [string]$games[0].Id,
            '--save-key', $script:preferences.save_key, '--restore-key', $script:preferences.restore_key,
            '--gui-session', $script:sessionPath, '--settings-file', $settingsPath,
            '--offline-launch-confirmed', '--allow-writes'
        )
        if (-not $script:preferences.sound) { $arguments += '--mute' }
        $quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $script:lastSequence = -1
        $script:lastSnapshot = $null
        $logDirectory = Join-Path $projectRoot 'logs'
        [IO.Directory]::CreateDirectory($logDirectory) | Out-Null
        $logName = 'gui-' + (Split-Path -Leaf $script:sessionPath)
        $script:child = Start-Process $PythonPath -ArgumentList $quoted -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDirectory "$logName.log") -RedirectStandardError (Join-Path $logDirectory "$logName.error.log")
        Set-Status 'Waiting for driving' 'LMU detected. Waiting for offline Practice.'
    } catch {
        if ($SmokeTest) {
            $script:smokeFailure = $_.Exception.Message
            Request-Exit
            return
        }
        Set-Status 'Connection error' $_.Exception.Message 'Signal'
        # Keep the timer alive so closing can still cooperatively stop the child.
    }
})
$window.Add_Closing({
    param($sender, $event)
    if ($script:canClose) { return }
    $event.Cancel = $true
    if ($script:closingRequested) { return }
    Show-SectorModal 'close'
})
$window.Add_Closed({
    $timer.Stop()
    $application.Shutdown()
})
$window.Add_Loaded({ $ui.SaveKey.Focus() | Out-Null })
# A hidden main window must keep the dispatcher and the host heartbeat alive.
$application = [Windows.Application]::new()
$application.ShutdownMode = [Windows.ShutdownMode]::OnExplicitShutdown
if ($SmokeTest) { $window.Opacity = 0; $window.ShowInTaskbar = $false }
try {
    $timer.Start()
    $application.Run($window) | Out-Null
} finally {
    $timer.Stop()
    Request-Stop
    if ($script:audioPlayer) { $script:audioPlayer.Dispose() }
    if ($script:audioProcess) { $script:audioProcess.Dispose() }
    $script:tray.Visible = $false
    $script:tray.Icon.Dispose()
    $script:tray.Dispose()
    $trayMenu.Dispose()
    Close-ResetGuiInstance $script:instance
}
if ($SmokeTest) {
    if ($script:smokeFailure) { throw $script:smokeFailure }
    if ($script:smokeStep -ne 4) { throw 'Window smoke test ended prematurely.' }
    Write-Output 'PASS: tabs, shortcut capture, embedded dialogs, scrolling, background/quit and controller status.'
}
