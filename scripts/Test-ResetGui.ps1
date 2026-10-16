param([switch]$IncludeWpf)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResetGui.Settings.ps1')
. (Join-Path $PSScriptRoot 'ResetGui.Status.ps1')
. (Join-Path $PSScriptRoot 'ResetGui.Instance.ps1')
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('reset-gui-test-' + [guid]::NewGuid().ToString('N'))
$path = Join-Path $testRoot 'settings.json'
function Assert-True([bool]$Value, [string]$Message) {
    if (-not $Value) { throw $Message }
}
try {
    $defaults = Read-ResetSettings $path
    Assert-True ($defaults.save_key -eq 'F6' -and $defaults.restore_key -eq 'F7' -and $defaults.sound) 'First-run defaults'
    $settings = [pscustomobject]@{ save_key = 'F8'; restore_key = 'F12'; sound = $false }
    Save-ResetSettings $path $settings
    $loaded = Read-ResetSettings $path
    Assert-True ($loaded.save_key -eq 'F8' -and $loaded.restore_key -eq 'F12' -and -not $loaded.sound) 'Settings round-trip'
    $settings.save_key = 'F12'
    Assert-True (-not (Test-ResetSettings $settings)) 'Duplicate key rejection'
    $settings.save_key = 'F25'
    Assert-True (-not (Test-ResetSettings $settings)) 'Out-of-range rejection'
    $settings.save_key = 'F1'
    Save-ResetSettings $path $settings
    Assert-True ((Read-ResetSettings $path).save_key -eq 'F1') 'Atomic replacement'
    $settings.save_key = 'G'
    $settings.restore_key = 'Space'
    Save-ResetSettings $path $settings
    Assert-True ((Read-ResetSettings $path).restore_key -eq 'Space') 'Captured key persistence'
    $settings.restore_key = 'g'
    Assert-True (-not (Test-ResetSettings $settings)) 'Case-insensitive duplicate rejection'
    $settings.restore_key = 'Ctrl+G'
    Assert-True (-not (Test-ResetSettings $settings)) 'Modifier combination rejection'
    [IO.File]::WriteAllText($path, '{broken')
    Assert-True ((Read-ResetSettings $path).save_key -eq 'F6') 'Malformed settings recovery'
    $xml = [xml][IO.File]::ReadAllText((Join-Path (Split-Path -Parent $PSScriptRoot) 'src/ui/reset_gui.xaml'))
    Assert-True ($null -ne $xml.Window) 'XAML is well-formed'
    Assert-True ($xml.Window.Title -eq 'Sector') 'Sector branding'
    $active = Get-ResetStatusView ([pscustomobject]@{
        phase = 'active'; checkpoint = $true; saved_speed_kmh = 80.0; restore_count = 2; message = 'Checkpoint applied.'
    })
    Assert-True ($active.Title -eq 'Connected' -and $active.Summary -eq '80 km/h saved  /  2 restores') 'Acknowledged checkpoint presentation'
    $paused = Get-ResetStatusView ([pscustomobject]@{ phase = 'paused'; checkpoint = $true; restore_count = 0 })
    Assert-True ($paused.Checkpoint -eq 'Checkpoint saved' -and $paused.Title -eq 'Waiting for driving') 'Pause retains checkpoint'
    $failed = Get-ResetStatusView ([pscustomobject]@{ phase = 'error'; message = 'Native cleanup failed.' })
    Assert-True ($failed.Tone -eq 'Signal' -and $failed.Detail -eq 'Native cleanup failed.') 'Native failures reach the UI'
    $stopped = Get-ResetStatusView ([pscustomobject]@{ phase = 'stopped'; checkpoint = $false })
    Assert-True ($stopped.Title -eq 'Disconnected' -and $stopped.Checkpoint -eq 'No checkpoint') 'Stopped state is not connected'
    Assert-True ($xml.SelectNodes('//*[local-name()="ComboBox" or local-name()="Popup"]').Count -eq 0) 'No dropdown or separate popup windows'
    $instanceName = 'Local\Sector.Desktop.Test.' + [guid]::NewGuid().ToString('N')
    $instance = Open-ResetGuiInstance $instanceName
    try {
        Assert-True $instance.IsOwner 'First launcher owns the session'
        $instanceScript = (Join-Path $PSScriptRoot 'ResetGui.Instance.ps1').Replace("'", "''")
        $childCode = ". '$instanceScript'; `$instance = Open-ResetGuiInstance '$instanceName'; `$owner = `$instance.IsOwner; Close-ResetGuiInstance `$instance; if (`$owner) { exit 1 }"
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($childCode))
        $child = Start-Process powershell.exe -ArgumentList "-NoProfile -EncodedCommand $encoded" -WindowStyle Hidden -PassThru
        try {
            Assert-True ($child.WaitForExit(10000)) 'Second launcher exits promptly'
            Assert-True ($child.ExitCode -eq 0) 'Second launcher cannot own another session'
            Assert-True ($instance.ShowRequest.WaitOne(1000)) 'Second launcher requests the existing window'
        } finally { $child.Dispose() }
    } finally { Close-ResetGuiInstance $instance }
    $reopened = Open-ResetGuiInstance $instanceName
    try { Assert-True $reopened.IsOwner 'Session can restart after exit' }
    finally { Close-ResetGuiInstance $reopened }
    foreach ($script in @(Get-ChildItem $PSScriptRoot -Filter '*ResetGui*.ps1')) {
        $parseErrors = $null
        [Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$null, [ref]$parseErrors) | Out-Null
        Assert-True ($parseErrors.Count -eq 0) "PowerShell syntax: $($script.Name)"
    }
    if ($IncludeWpf) {
        & (Join-Path $PSScriptRoot 'Start-ResetGui.ps1') -ValidateOnly
        & powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Start-ResetGui.ps1') -Preview -SmokeTest
        Assert-True ($LASTEXITCODE -eq 0) 'WPF window lifecycle smoke test'
    }
    Write-Output 'PASS: GUI settings persistence, invalid-key rejection, recovery and XAML.'
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (-not $resolvedTestRoot.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path -Leaf $resolvedTestRoot) -notmatch '^reset-gui-test-[0-9a-f]{32}$') {
            throw 'Refusing cleanup outside the generated GUI test directory.'
        }
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}
