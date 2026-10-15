param(
    [string]$PythonPath,
    [switch]$Preview,
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'ResetGui.Settings.ps1')
Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ResetWindowTheme {
    [DllImport("dwmapi.dll")]
    public static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
}
'@

$reader = [Xml.XmlReader]::Create((Join-Path $projectRoot 'src/reset_gui.xaml'))
try { $window = [Windows.Markup.XamlReader]::Load($reader) } finally { $reader.Close() }
$window.Add_SourceInitialized({
    try {
        $handle = [Windows.Interop.WindowInteropHelper]::new($window).Handle
        $dark = 1
        [ResetWindowTheme]::DwmSetWindowAttribute($handle, 20, [ref]$dark, 4) | Out-Null
    } catch { } # Older Windows still renders the dark content with native chrome.
})
$ui = @{}
foreach ($name in @('LayoutRoot', 'StatusDot', 'StatusTitle', 'CheckpointState', 'LastEvent',
    'StatusDetail', 'SaveKey', 'RestoreKey', 'KeyHint', 'SoundEnabled', 'PreviewSave',
    'PreviewRestore', 'SoundPreviews', 'SettingsHint', 'OpenLogs')) {
    $ui[$name] = $window.FindName($name)
    if ($null -eq $ui[$name]) { throw "Missing UI element: $name" }
}
foreach ($number in 1..24) {
    $ui.SaveKey.Items.Add("F$number") | Out-Null
    $ui.RestoreKey.Items.Add("F$number") | Out-Null
}
if ($ValidateOnly) {
    $ui.SaveKey.SelectedItem = 'F6'
    $ui.RestoreKey.SelectedItem = 'F7'
    $window.Measure([Windows.Size]::new(580, 650))
    $window.Arrange([Windows.Rect]::new(0, 0, 580, 650))
    $window.UpdateLayout()
    Write-Output 'PASS: WPF window, named controls, dropdowns and layout loaded.'
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
$ui.SaveKey.SelectedItem = $settings.save_key
$ui.RestoreKey.SelectedItem = $settings.restore_key
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

function Save-Preferences {
    $candidate = [pscustomobject]@{
        save_key = [string]$ui.SaveKey.SelectedItem
        restore_key = [string]$ui.RestoreKey.SelectedItem
        sound = [bool]$ui.SoundEnabled.IsChecked
    }
    if (-not (Test-ResetSettings $candidate)) {
        $ui.KeyHint.Text = "Choose different keys. Current bindings: $($script:preferences.save_key) / $($script:preferences.restore_key)."
        $ui.KeyHint.SetResourceReference([Windows.Controls.TextBlock]::ForegroundProperty, 'Signal')
        $ui.SettingsHint.Text = 'Changes not saved'
        return
    }
    try {
        if (-not $Preview) { Save-ResetSettings $settingsPath $candidate }
        $script:preferences = $candidate
        $ui.KeyHint.Text = 'Choose keys that are not assigned in LMU.'
        $ui.KeyHint.SetResourceReference([Windows.Controls.TextBlock]::ForegroundProperty, 'Secondary')
        $ui.SettingsHint.Text = 'Settings saved automatically'
        $ui.PreviewSave.IsEnabled = $candidate.sound
        $ui.PreviewRestore.IsEnabled = $candidate.sound
    } catch {
        $ui.SettingsHint.Text = 'Settings could not be saved'
        $ui.SettingsHint.ToolTip = $_.Exception.Message
    }
}
$ui.SaveKey.Add_SelectionChanged({ Save-Preferences })
$ui.RestoreKey.Add_SelectionChanged({ Save-Preferences })
$ui.SoundEnabled.Add_Checked({ Save-Preferences })
$ui.SoundEnabled.Add_Unchecked({ Save-Preferences })
Save-Preferences

function Play-Preview([string]$Name) {
    $path = Join-Path $audioDirectory "$Name.wav"
    if (-not [IO.File]::Exists($path)) {
        $ui.SettingsHint.Text = 'Sound preview is not ready'
        return
    }
    try {
        if ($script:audioPlayer) { $script:audioPlayer.Dispose() }
        $script:audioPlayer = [Media.SoundPlayer]::new($path)
        $script:audioPlayer.Play()
    } catch { $ui.SettingsHint.Text = 'Audio device unavailable' }
}
$ui.PreviewSave.Add_Click({ Play-Preview 'save' })
$ui.PreviewRestore.Add_Click({ Play-Preview 'restore' })
$ui.OpenLogs.Add_Click({
    $logDirectory = Join-Path $projectRoot 'logs'
    [IO.Directory]::CreateDirectory($logDirectory) | Out-Null
    Start-Process explorer.exe -ArgumentList ('"' + $logDirectory + '"')
})

function Set-Status([string]$Title, [string]$Detail, [string]$Tone = 'Secondary') {
    $ui.StatusTitle.Text = $Title
    $ui.StatusDetail.Text = $Detail
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
    $ui.CheckpointState.Text = if ($snapshot.checkpoint) { 'Checkpoint saved' } else { 'No checkpoint' }
    if ($snapshot.message) { $ui.LastEvent.Text = $snapshot.message }
    switch ($snapshot.phase) {
        'waiting' { Set-Status 'Waiting for driving' 'LMU detected. Waiting for offline Practice.' }
        'starting' { Set-Status 'Connecting' 'Checking the game and vehicle.' }
        'active' { Set-Status 'Connected' 'Checkpoint stays available until the vehicle is reset.' 'Accent' }
        'paused' {
            $detail = if ($snapshot.checkpoint) { 'Checkpoint retained while driving is paused.' } else { 'Waiting for offline Practice driving.' }
            Set-Status 'Waiting for driving' $detail
        }
        'stopping' { Set-Status 'Disconnecting' 'Finishing the current operation.' }
        'stopped' { Set-Status 'Waiting for LMU' 'The app connects automatically.' }
        'error' { Set-Status 'Unable to connect' $snapshot.message 'Signal' }
    }
}

function Request-Stop {
    if ($script:child -and -not $script:child.HasExited) {
        [IO.File]::WriteAllText((Join-Path $script:sessionPath 'stop'), '')
        Set-Status 'Disconnecting' 'Finishing the current operation.'
    }
}

if (-not $Preview) {
    try {
        if (-not $PythonPath) {
            $bundled = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
            if (Test-Path -LiteralPath $bundled) { $PythonPath = $bundled }
            else { $PythonPath = (Get-Command python -ErrorAction Stop).Source }
        }
        [IO.Directory]::CreateDirectory($audioDirectory) | Out-Null
        $audioArgs = '"{0}" "{1}"' -f (Join-Path $projectRoot 'src/reset_feedback.py'), $audioDirectory
        $script:audioProcess = Start-Process $PythonPath -ArgumentList $audioArgs -WindowStyle Hidden -PassThru
    } catch {
        $script:startupError = 'Python is unavailable. Install Python 3.11+ or pass -PythonPath.'
        Set-Status 'Setup required' $script:startupError 'Signal'
    }
} else {
    $window.Title = 'Active Reset - Preview'
    Set-Status 'Waiting for LMU' 'The app connects automatically when you start driving.'
}

$timer = [Windows.Threading.DispatcherTimer]::new()
$timer.Interval = [timespan]::FromMilliseconds(250)
$timer.Add_Tick({
    try {
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
                if ($exitCode -ne 0 -and $script:lastSnapshot.phase -ne 'error') {
                    Set-Status 'Unable to connect' 'Open logs for details. Reopen Active Reset after fixing the issue.' 'Signal'
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
            $ui.LastEvent.Text = "Press $($script:preferences.save_key) on track to save a checkpoint."
            return
        }
        $gameIdentity = "$($games[0].Id):$($games[0].StartTime.ToUniversalTime().Ticks)"
        if ($gameIdentity -eq $script:blockedIdentity) { return }
        $script:blockedIdentity = $gameIdentity
        $script:sessionPath = Join-Path $projectRoot ('work/gui/' + [guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($script:sessionPath) | Out-Null
        [IO.File]::WriteAllText((Join-Path $script:sessionPath 'heartbeat'), '')
        $arguments = @(
            (Join-Path $projectRoot 'src/start_reset_hotkeys.py'), '--pid', [string]$games[0].Id,
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
        Set-Status 'Connection error' $_.Exception.Message 'Signal'
        # Keep the timer alive so closing can still cooperatively stop the child.
    }
})
$window.Add_Closing({
    param($sender, $event)
    if ($script:canClose -or -not $script:child -or $script:child.HasExited) { return }
    $event.Cancel = $true
    $script:closingRequested = $true
    Request-Stop
})
$window.Add_Closed({
    $timer.Stop()
    if ($script:audioPlayer) { $script:audioPlayer.Dispose() }
    if ($script:audioProcess) { $script:audioProcess.Dispose() }
})
$window.Add_Loaded({ $ui.SaveKey.Focus() | Out-Null })
$timer.Start()
$window.ShowDialog() | Out-Null
