param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [string]$FrameDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$work = Join-Path $projectRoot 'work'
[IO.Directory]::CreateDirectory($work) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $projectRoot 'bin')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $projectRoot 'compatibility')) | Out-Null
if (@(Get-Process -Name 'Le Mans Ultimate', 'diagnostic-14200' -ErrorAction SilentlyContinue).Count) {
    throw 'Close LMU and Sector before rebuilding their local binaries.'
}
$profilePath = Join-Path $projectRoot 'compatibility/steam-25661166-telemetry.json'
$profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
if ($profile.observation_build -ne 14200 -or $profile.physics_compatible -ne $false) {
    throw 'Unsupported pinned telemetry profile.'
}
foreach ($entry in $profile.files) {
    if ((Get-FileHash -LiteralPath (Join-Path $profile.install_path $entry.relative_path) -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw 'Installed file differs from the pinned telemetry profile.'
    }
}
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $work 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $work 'zig-local-cache'
$diagnostic = Join-Path $work 'diagnostic-14200-desktop.exe'
& $ZigPath c++ -std=c++17 -O2 -target x86_64-windows-gnu -I (Join-Path $profile.install_path 'Support/SharedMemoryInterface') (Join-Path $projectRoot 'src/native/telemetry/diagnostic.cpp') -o $diagnostic
if ($LASTEXITCODE -ne 0) { throw 'Telemetry build failed.' }
& $diagnostic --schema
if ($LASTEXITCODE -ne 0) { throw 'Telemetry schema check failed.' }
& $diagnostic --self-test
if ($LASTEXITCODE -ne 0) { throw 'Telemetry self-test failed.' }
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'test/Test-Core.ps1') -ZigPath $ZigPath
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'build/Build-PhysicalResetHost25661166.ps1') -ZigPath $ZigPath -FrameDirectory $FrameDirectory
Copy-Item -LiteralPath $diagnostic -Destination (Join-Path $projectRoot 'bin/diagnostic-14200.exe')
$profile.diagnostic_sha256 = (Get-FileHash $diagnostic -Algorithm SHA256).Hash
$profile | Add-Member -NotePropertyName continuous_capture -NotePropertyValue $true -Force
[IO.File]::WriteAllText($profilePath, ($profile | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'build/Build-OverlayReader.ps1') -ZigPath $ZigPath
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'build/Build-SectorUi.ps1')
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'test/Test-ResetGui.ps1') -IncludeElectron
Write-Output 'Desktop build ready. Run ./scripts/launch/Start-ResetGui.ps1 to open Sector.'
