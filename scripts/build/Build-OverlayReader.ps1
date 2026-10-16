param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [string]$SdkPath
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $SdkPath) {
    $profile = Get-Content -LiteralPath (Join-Path $projectRoot 'compatibility/steam-25661166-telemetry.json') -Raw | ConvertFrom-Json
    $SdkPath = Join-Path $profile.install_path 'Support/SharedMemoryInterface'
}
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $projectRoot 'work/zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $projectRoot 'work/zig-local-cache'
[IO.Directory]::CreateDirectory((Join-Path $projectRoot 'work')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $projectRoot 'bin')) | Out-Null
$output = Join-Path $projectRoot 'work/overlay-reader.exe'
& $ZigPath c++ -std=c++17 -O2 -target x86_64-windows-gnu -I $SdkPath (Join-Path $projectRoot 'src/native/telemetry/overlay_reader.cpp') -luser32 -o $output
if ($LASTEXITCODE -ne 0) { throw 'Overlay reader build failed.' }
& $output --self-test
if ($LASTEXITCODE -ne 0) { throw 'Overlay reader self-test failed.' }
Copy-Item -LiteralPath $output -Destination (Join-Path $projectRoot 'bin/overlay-reader.exe')
