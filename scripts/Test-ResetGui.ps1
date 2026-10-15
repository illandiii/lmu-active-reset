param([switch]$IncludeWpf)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ResetGui.Settings.ps1')
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
    [IO.File]::WriteAllText($path, '{broken')
    Assert-True ((Read-ResetSettings $path).save_key -eq 'F6') 'Malformed settings recovery'
    $xml = [xml][IO.File]::ReadAllText((Join-Path (Split-Path -Parent $PSScriptRoot) 'src/reset_gui.xaml'))
    Assert-True ($null -ne $xml.Window) 'XAML is well-formed'
    if ($IncludeWpf) { & (Join-Path $PSScriptRoot 'Start-ResetGui.ps1') -ValidateOnly }
    Write-Output 'PASS: GUI settings persistence, invalid-key rejection, recovery and XAML.'
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
