param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$telemetryWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$telemetryProfile=Get-Content -LiteralPath "$projectRoot\compatibility\14200-telemetry.json" -Raw | ConvertFrom-Json
foreach($entry in $telemetryProfile.files) {
    if((Get-FileHash -LiteralPath (Join-Path $telemetryProfile.install_path $entry.relative_path)).Hash -ne $entry.sha256){throw 'Installed file differs from the 14200 profile'}
}
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $telemetryWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $telemetryWork 'zig-local-cache'
# Build to work first, preserving the known diagnostic if compilation fails.
& $ZigPath c++ -std=c++17 -O2 -target x86_64-windows-gnu -I (Join-Path $telemetryProfile.install_path 'Support\SharedMemoryInterface') "$projectRoot\src/native/telemetry/diagnostic.cpp" -o "$telemetryWork\diagnostic-14200.exe" 2> "$telemetryWork\telemetry-build.log"
if($LASTEXITCODE -ne 0){throw 'Telemetry compilation failed'}
& "$telemetryWork\diagnostic-14200.exe" --schema
if($LASTEXITCODE -ne 0){throw 'Schema failed'}
& "$telemetryWork\diagnostic-14200.exe" --self-test
if($LASTEXITCODE -ne 0){throw 'Diagnostic tests failed'}
Write-Output 'Rebuilt in work. Compare schema and hash before updating the pinned binary/profile.'
